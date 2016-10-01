#include "tcp.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <fstream>
#include <gflags/gflags.h>
#include <limits>
#include <string>
#include <type_traits>

#include "../common/substitute.h"
#include "../metrics/metrics.h"

DEFINE_bool(disable_tcp_retx_timer, false,
            "Disables TCP's retx timeout. This may cause long-lived "
            "connections to stall and short lived connections to never "
            "terminate.");

namespace ncode {
namespace htsim {

using namespace std::chrono;

static auto* kTCPSourceAcksRxMetric =
    metrics::DefaultMetricManager()
        -> GetUnsafeMetric<uint64_t, uint32_t, uint32_t, uint32_t, uint32_t>(
            "tcp_src_acks_rx", "ACKs received", "IP source ", "IP destination",
            "TCP source port", "TCP destination port");

static auto* kTCPSourceFastRetxMetric =
    metrics::DefaultMetricManager()
        -> GetUnsafeMetric<uint64_t, uint32_t, uint32_t, uint32_t, uint32_t>(
            "tcp_src_fast_retx", "Fast retransmissions", "IP source",
            "IP destination", "TCP source port", "TCP destination port");

static auto* kTCPSourceRetxTimeoutMetric =
    metrics::DefaultMetricManager()
        -> GetUnsafeMetric<uint64_t, uint32_t, uint32_t, uint32_t, uint32_t>(
            "tcp_src_retx_timeout", "Retransmission timeouts", "IP source",
            "IP destination", "TCP source port", "TCP destination port");

static auto* kTCPSourceCwndMetric =
    metrics::DefaultMetricManager()
        -> GetUnsafeMetric<uint32_t, uint32_t, uint32_t, uint32_t, uint32_t>(
            "tcp_src_cwnd", "Congestion window size", "IP source",
            "IP destination", "TCP source port", "TCP destination port");

static auto* kTCPSourceHighestSequenceSentMetric =
    metrics::DefaultMetricManager()
        -> GetUnsafeMetric<uint64_t, uint32_t, uint32_t, uint32_t, uint32_t>(
            "tcp_src_highest_sequence_sent", "Highest sequence number sent",
            "IP source", "IP destination", "TCP source port",
            "TCP destination port");

static auto* kTCPSourceSendBufferMetric =
    metrics::DefaultMetricManager()
        -> GetUnsafeMetric<uint64_t, uint32_t, uint32_t, uint32_t, uint32_t>(
            "tcp_src_send_buffer", "Outstanding data to send", "IP source",
            "IP destination", "TCP source port", "TCP destination port");

static auto* kTCPSinkIncomingTagChangesMetric =
    metrics::DefaultMetricManager()
        -> GetUnsafeMetric<uint32_t, uint32_t, uint32_t, uint32_t, uint32_t>(
            "tcp_sink_incoming_tag_changes",
            "The number of times incoming tags have changed", "IP source",
            "IP destination", "TCP source port", "TCP destination port");

static auto* kTCPSourceCompletionTimesMetric =
    metrics::DefaultMetricManager()
        -> GetUnsafeMetric<uint64_t, uint32_t, uint32_t, uint32_t, uint32_t>(
            "tcp_source_completion_times",
            "Completion time for each TCP flow as measured by the source",
            "IP source", "IP destination", "TCP source port",
            "TCP destination port");

static auto* kTCPSinkBytesRx =
    metrics::DefaultMetricManager()
        -> GetUnsafeMetric<uint64_t, uint32_t, uint32_t, uint32_t, uint32_t>(
            "tcp_sink_bytes_rx",
            "Bytes received by the sink up to a point in time", "IP source",
            "IP destination", "TCP source port", "TCP destination port");

// Size of an ACK packet.
static constexpr size_t kAckSize = 40;

TCPSource::TCPSource(const std::string& id, const net::FiveTuple& five_tuple,
                     uint16_t mss, uint32_t maxcwnd, PacketHandler* out,
                     EventQueue* event_queue, bool important)
    : Connection(id, five_tuple, out, event_queue),
      mss_(mss),
      maxcwnd_(maxcwnd),
      fast_retx_metric_(nullptr),
      retx_timeout_metric_(nullptr) {
  AddMetrics(important);
  Close();
}

void TCPSource::Close() {
  last_sent_time_ = EventQueueTime::ZeroTime();
  first_sent_time_ = EventQueueTime::MaxTime();
  highest_seqno_sent_ = 0;
  highest_seqno_sent_real_ = 0;
  cwnd_ = 0;
  last_acked_ = 0;
  dupacks_ = 0;
  ssthresh_ = 0xffffffff;
  rtt_ = 0;
  mdev_ = 0;
  rtt_avg_ = 0;
  rtt_cum_ = 0;
  sawtooth_ = 0;
  recoverq_ = 0;
  in_fast_recovery_ = false;
  send_buffer_ = 0;
  rto_ = event_queue_->ToTime(seconds(1)).Raw();
}

void TCPSource::AddMetrics(bool important) {
  fast_retx_metric_ = kTCPSourceFastRetxMetric->GetHandle(
      five_tuple_.ip_src().Raw(), five_tuple_.ip_dst().Raw(),
      five_tuple_.src_port().Raw(), five_tuple_.dst_port().Raw());
  retx_timeout_metric_ = kTCPSourceRetxTimeoutMetric->GetHandle(
      five_tuple_.ip_src().Raw(), five_tuple_.ip_dst().Raw(),
      five_tuple_.src_port().Raw(), five_tuple_.dst_port().Raw());
  completion_times_metric_ = kTCPSourceCompletionTimesMetric->GetHandle(
      five_tuple_.ip_src().Raw(), five_tuple_.ip_dst().Raw(),
      five_tuple_.src_port().Raw(), five_tuple_.dst_port().Raw());

  if (important) {
    kTCPSourceAcksRxMetric->GetHandle(
        [this] { return last_acked_; }, five_tuple_.ip_src().Raw(),
        five_tuple_.ip_dst().Raw(), five_tuple_.src_port().Raw(),
        five_tuple_.dst_port().Raw());
    kTCPSourceCwndMetric->GetHandle(
        [this] { return cwnd_; }, five_tuple_.ip_src().Raw(),
        five_tuple_.ip_dst().Raw(), five_tuple_.src_port().Raw(),
        five_tuple_.dst_port().Raw());
    kTCPSourceSendBufferMetric->GetHandle(
        [this] { return send_buffer_; }, five_tuple_.ip_src().Raw(),
        five_tuple_.ip_dst().Raw(), five_tuple_.src_port().Raw(),
        five_tuple_.dst_port().Raw());
    kTCPSourceHighestSequenceSentMetric->GetHandle(
        [this] { return highest_seqno_sent_; }, five_tuple_.ip_src().Raw(),
        five_tuple_.ip_dst().Raw(), five_tuple_.src_port().Raw(),
        five_tuple_.dst_port().Raw());
  }
}

void TCPSource::UpdateCompletionTime() {
  EventQueueTime completion_time =
      event_queue_->CurrentTime() - first_sent_time_;
  uint64_t completion_time_ms = event_queue_->TimeToRawMillis(completion_time);
  completion_times_metric_->AddValue(completion_time_ms);
}

void TCPSource::ReceivePacket(PacketPtr pkt) {
  // The packet must be a TCP ack. We only know how to handle ACKs (all flows
  // are unidirectional).
  const TCPPacket* ack_packet = static_cast<const TCPPacket*>(pkt.get());

  uint64_t seqno = ack_packet->sequence().Raw();
  EventQueueTime time_ack_sent = ack_packet->time_sent();
  if (time_ack_sent < first_sent_time_) {
    // Ignore the ACK.
    return;
  }

  if (seqno < last_acked_) {
    // Treat it as a dupack.
    seqno = last_acked_;
  }

  // Compute RTT
  int64_t m = (event_queue_->CurrentTime() - time_ack_sent).Raw();
  if (m != 0) {
    if (rtt_ > 0) {
      m -= (rtt_ >> 3);
      rtt_ += m;

      if (m < 0) {
        m = -m;
        m -= mdev_ >> 2;
        if (m > 0) {
          m >>= 3;
        }
      } else {
        m -= mdev_ >> 2;
      }

      mdev_ += m;
    } else {
      rtt_ = m << 3;
      mdev_ = m << 1;
    }
  }

  rto_ = (rtt_ >> 3) + mdev_;

  // In Linux the RTO is at least Hz/5 (200ms).
  EventQueueTime min_rtt = event_queue_->ToTime(milliseconds(200));
  if (rto_ < min_rtt.Raw()) {
    rto_ = min_rtt.Raw();
  }

  uint64_t t1 = event_queue_->ToTime(seconds(5)).Raw();
  uint64_t t2 = event_queue_->ToTime(seconds(2)).Raw();
  if (rto_ > t1) {
    rto_ = t2;
  }

  if (seqno > last_acked_) {  // a brand new ack
    // Best behavior: proper ack of a new packet, when we were expecting it
    if (!in_fast_recovery_) {
      last_acked_ = seqno;
      dupacks_ = 0;
      InflateWindow();
      SendPackets();
      return;
    }

    // We're in fast recovery, i.e. one packet has been
    // dropped but we're pretending it's not serious
    if (seqno >= recoverq_) {
      // got ACKs for all the "recovery window": resume
      // normal service
      uint32_t flightsize = highest_seqno_sent_ - seqno;
      cwnd_ = std::min(ssthresh_, flightsize + mss_);
      last_acked_ = seqno;
      dupacks_ = 0;
      in_fast_recovery_ = false;
      SendPackets();
      return;
    }

    // In fast recovery, and still getting ACKs for the
    // "recovery window"
    // This is dangerous. It means that several packets
    // got lost, not just the one that triggered FR.
    uint32_t new_data = seqno - last_acked_;
    last_acked_ = seqno;
    if (new_data < cwnd_) {
      cwnd_ -= new_data;
    } else {
      cwnd_ = 0;
    }

    cwnd_ += mss_;

    RetransmitPacket();
    fast_retx_metric_->AddValue(last_acked_ + 1);

    SendPackets();
    return;
  }

  // It's a dup ack
  if (in_fast_recovery_) {
    // Still in fast recovery; hopefully the prodigal ACK is on it's way.
    cwnd_ += mss_;
    if (cwnd_ > maxcwnd_) {
      cwnd_ = maxcwnd_;
    }
    SendPackets();
    return;
  }

  // Not yet in fast recovery. What should we do instead?
  dupacks_++;
  if (dupacks_ != 3) {  // not yet serious worry
    SendPackets();
    return;
  }

  if (last_acked_ < recoverq_) {  // See RFC 3782: if we haven't
    // recovered from timeouts
    // etc. don't do fast recovery
    //    std::cout << "RFC 3782, retx suppressed\n";
    return;
  }

  // begin fast recovery
  ssthresh_ = std::max(cwnd_ / 2, static_cast<uint32_t>(2 * mss_));

  if (sawtooth_ > 0) {
    rtt_avg_ = rtt_cum_ / sawtooth_;
  } else {
    rtt_avg_ = 0;
  }

  sawtooth_ = 0;
  rtt_cum_ = 0;

  //  std::cout << "fast retx " << (last_acked_ + 1) << "\n";

  RetransmitPacket();
  fast_retx_metric_->AddValue(last_acked_ + 1);

  cwnd_ = ssthresh_ + 3 * mss_;
  in_fast_recovery_ = true;
  recoverq_ = highest_seqno_sent_;  // _recoverq is the value of the
  // first ACK that tells us things
  // are back on track
}

void TCPSource::InflateWindow() {
  int newly_acked = (last_acked_ + cwnd_) - highest_seqno_sent_;
  // be very conservative - possibly not the best we can do, but
  // the alternative has bad side effects.
  if (newly_acked > mss_) {
    newly_acked = mss_;
  }
  if (newly_acked < 0) {
    return;
  }

  if (cwnd_ < ssthresh_) {  // slow start
    uint32_t increase =
        std::min(ssthresh_ - cwnd_, static_cast<uint32_t>(newly_acked));
    cwnd_ += increase;

    if (cwnd_ > maxcwnd_) {
      cwnd_ = maxcwnd_;
    }

    newly_acked -= increase;
  }

  // additive increase
  else {
    uint32_t pkts = cwnd_ / mss_;

    uint32_t increase = (newly_acked * mss_) / cwnd_;
    if (increase == 0) {
      increase = 1;
    }

    cwnd_ += increase;  // XXX beware large windows
    if (pkts != cwnd_ / mss_) {
      rtt_cum_ += rtt_;
      sawtooth_++;
    }
  }
}

void TCPSource::RtxTimerHook(EventQueueTime now) {
  if (highest_seqno_sent_ == 0) {
    return;
  }

  if (last_acked_ >= highest_seqno_sent_real_) {
    if (on_send_buffer_drained_) {
      UpdateCompletionTime();
      on_send_buffer_drained_();
      on_send_buffer_drained_ = nullptr;
    }

    return;
  }

  if (now.Raw() <= last_sent_time_.Raw() + rto_) {
    return;
  }

  //  LOG(ERROR) << "Retx timeout last sent "
  //             << event_queue_->TimeToRawMillis(last_sent_time_) << " rto "
  //             << event_queue_->TimeToRawMillis(
  //                    ncode::common::EventQueueTime(rto_))
  //             << " at " << id();

  if (in_fast_recovery_) {
    uint32_t flightsize = highest_seqno_sent_ - last_acked_;
    cwnd_ = std::min(ssthresh_, flightsize + mss_);
  }

  ssthresh_ = std::max(cwnd_ / 2, static_cast<uint32_t>(2 * mss_));

  if (sawtooth_ > 0) {
    rtt_avg_ = rtt_cum_ / sawtooth_;
  } else {
    rtt_avg_ = 0;
  }

  sawtooth_ = 0;
  rtt_cum_ = 0;

  cwnd_ = mss_;
  in_fast_recovery_ = false;
  recoverq_ = highest_seqno_sent_;
  highest_seqno_sent_ = last_acked_ + mss_;
  dupacks_ = 0;

  RetransmitPacket();
  retx_timeout_metric_->AddValue(last_acked_ + 1);
}

void TCPSource::RetransmitPacket() {
  EventQueueTime now = event_queue_->CurrentTime();
  auto pkt_ptr =
      make_unique<TCPPacket>(five_tuple_, mss_, now, SeqNum(last_acked_ + 1));

  last_sent_time_ = now;
  SendPacket(std::move(pkt_ptr));
}

void TCPSource::SendPackets() {
  if (last_acked_ >= highest_seqno_sent_real_ && send_buffer_ == 0 &&
      on_send_buffer_drained_) {
    UpdateCompletionTime();
    on_send_buffer_drained_();
    on_send_buffer_drained_ = nullptr;
    return;
  }

  EventQueueTime now = event_queue_->CurrentTime();
  while (last_acked_ + cwnd_ >= highest_seqno_sent_ + mss_) {
    if (send_buffer_ == 0) {
      break;
    }

    if (highest_seqno_sent_ == 0) {
      first_sent_time_ = now;
    }

    size_t to_tx = std::min(static_cast<uint64_t>(mss_), send_buffer_);
    auto pkt_ptr = make_unique<TCPPacket>(five_tuple_, to_tx, now,
                                          SeqNum(highest_seqno_sent_ + 1));

    send_buffer_ -= to_tx;
    highest_seqno_sent_ += to_tx;  // XX beware wrapping
    highest_seqno_sent_real_ += to_tx;

    last_sent_time_ = now;
    SendPacket(std::move(pkt_ptr));
  }
}

void TCPSource::AddData(uint64_t bytes) {
  if (send_buffer_ < mss_) {
    Close();
  }

  // If the flow does not have any outstanding data its congestion window will
  // be 0 (set by Close()). In that case we will set it to its initial value.
  if (cwnd_ == 0) {
    cwnd_ = kInitialCWNDMultiplier * mss_;
  }

  if (send_buffer_ + bytes < send_buffer_) {
    // Send buffer will overflow. Ignore the new data.
    send_buffer_ = std::numeric_limits<uint64_t>::max();
  } else {
    send_buffer_ += bytes;
  }

  SendPackets();
}

TCPRtxTimer::TCPRtxTimer(const std::string& id, EventQueueTime scan_period,
                         EventQueue* event_queue)
    : SimComponent(id, event_queue),
      EventConsumer(id, event_queue),
      scan_period_(scan_period) {}

void TCPRtxTimer::HandleEvent() {
  EventQueueTime now = event_queue_->CurrentTime();
  for (const auto& tcp_ptr : tcp_sources_) {
    tcp_ptr->RtxTimerHook(now);
  }

  EnqueueIn(scan_period_);
}

void TCPRtxTimer::RegisterTCPSource(TCPSource* tcp_source) {
  if (FLAGS_disable_tcp_retx_timer) {
    return;
  }

  tcp_sources_.emplace_back(tcp_source);
  if (tcp_sources_.size() == 1) {
    EnqueueIn(scan_period_);
  }
}

void TCPSink::AddMetrics() {
  incoming_tag_changes_metric_ = kTCPSinkIncomingTagChangesMetric->GetHandle(
      five_tuple_.ip_src().Raw(), five_tuple_.ip_dst().Raw(),
      five_tuple_.src_port().Raw(), five_tuple_.dst_port().Raw());
}

void TCPSink::Reset() {
  cumulative_ack_ = 0;
  last_seen_incoming_tag_ = PacketTag::Max();
  received_.clear();
}

void TCPSink::ReceivePacket(PacketPtr pkt) {
  using namespace std::chrono;

  const TCPPacket* tcp_packet = static_cast<const TCPPacket*>(pkt.get());

  uint64_t seqno = tcp_packet->sequence().Raw();
  size_t size_bytes = pkt->size_bytes();
  if (seqno == 1) {
    // This is the first packet in the flow.
    Reset();
  }

  if (last_seen_incoming_tag_ != pkt->tag()) {
    ++tag_change_count_;
    incoming_tag_changes_metric_->AddValue(tag_change_count_);
    last_seen_incoming_tag_ = pkt->tag();
  }

  if (seqno == cumulative_ack_ + 1) {  // it's the next expected seq no
    cumulative_ack_ = seqno + size_bytes - 1;
    // are there any additional received packets we can now ack?
    while (!received_.empty() && (received_.front() == cumulative_ack_ + 1)) {
      received_.pop_front();
      cumulative_ack_ += size_bytes;
    }
  } else if (seqno < cumulative_ack_ + 1) {
  }       // must have been a bad retransmit
  else {  // it's not the next expected sequence number
    if (received_.empty()) {
      received_.push_front(seqno);
    } else if (seqno > received_.back()) {  // likely case
      received_.push_back(seqno);
    } else {  // uncommon case - it fills a hole
      for (auto it = received_.begin(); it != received_.end(); ++it) {
        if (seqno == (*it)) break;  // it's a bad retransmit
        if (seqno < (*it)) {
          received_.insert(it, seqno);
          break;
        }
      }
    }
  }

  SendAck(pkt->time_sent());
}

void TCPSink::SendAck(EventQueueTime time_sent) {
  auto pkt_ptr = make_unique<TCPPacket>(five_tuple_, kAckSize, time_sent,
                                        SeqNum(cumulative_ack_));
  SendPacket(std::move(pkt_ptr));
}

void TCPSink::RecordBytesReceived() {
  auto metric = kTCPSinkBytesRx->GetHandle(
      five_tuple_.ip_src().Raw(), five_tuple_.ip_dst().Raw(),
      five_tuple_.src_port().Raw(), five_tuple_.dst_port().Raw());
  metric->AddValue(cumulative_ack_);
}

}  // namespace htsim
}  // namespace ncode
