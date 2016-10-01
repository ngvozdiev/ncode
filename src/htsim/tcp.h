#ifndef NCODE_HTSIM_TCP_H
#define NCODE_HTSIM_TCP_H

#include <cstdint>
#include <iostream>
#include <list>
#include <memory>
#include <stdexcept>
#include <vector>

#include "../common/common.h"
#include "../common/logging.h"
#include "../metrics/metrics.h"
#include "packet.h"

namespace ncode {
namespace htsim {

class TCPSource : public Connection {
 public:
  // How many packets to initially open the congestion window to.
  static constexpr size_t kInitialCWNDMultiplier = 4;

  TCPSource(const std::string& id, const net::FiveTuple& five_tuple,
            uint16_t mss, uint32_t maxcwnd, PacketHandler* out,
            EventQueue* event_queue, bool important = true);

  void ReceivePacket(PacketPtr pkt) override;

  void AddData(uint64_t data_bytes) override;

  void Close() override;

  void RtxTimerHook(EventQueueTime now);

 private:
  void UpdateCompletionTime();

  void InflateWindow();

  void RetransmitPacket();

  void SendPackets();

  void AddMetrics(bool important);

  const uint16_t mss_;
  const uint32_t maxcwnd_;
  EventQueueTime last_sent_time_;

  uint64_t highest_seqno_sent_;
  uint64_t highest_seqno_sent_real_;
  uint32_t cwnd_;
  uint64_t last_acked_;
  uint16_t dupacks_;
  uint32_t ssthresh_;

  uint64_t rtt_, rto_, mdev_;
  uint64_t rtt_avg_, rtt_cum_;
  int sawtooth_;

  uint64_t recoverq_;

  bool in_fast_recovery_;

  // How many bytes there are to be sent.
  uint64_t send_buffer_;

  // How long does it take to add one byte to the buffer.
  EventQueueTime time_to_add_byte_to_buffer_;

  // The sequence of every pkt retx as a fast retransmission is logged here.
  metrics::UnsafeMetricHandle<uint64_t>* fast_retx_metric_;

  // The sequence of every pkt retx due to a timeout is logged here.
  metrics::UnsafeMetricHandle<uint64_t>* retx_timeout_metric_;

  // Completion times for each flow.
  metrics::UnsafeMetricHandle<uint64_t>* completion_times_metric_;

  // Time the first packet in the flow is sent.
  EventQueueTime first_sent_time_;

  DISALLOW_COPY_AND_ASSIGN(TCPSource);
};

class TCPSink : public Connection {
 public:
  TCPSink(const std::string& id, const net::FiveTuple& five_tuple,
          PacketHandler* out_handler, EventQueue* event_queue)
      : Connection(id, five_tuple, out_handler, event_queue),
        cumulative_ack_(0),
        last_seen_incoming_tag_(PacketTag::Max()),
        tag_change_count_(0),
        incoming_tag_changes_metric_(nullptr) {
    AddMetrics();
  }

  void AddMetrics();

  void AddData(uint64_t bytes) override {
    Unused(bytes);
    LOG(FATAL) << "Attempted to add data to TCP sink.";
  }

  void Close() override { LOG(FATAL) << "Attempted to close a TCP sink."; }

  void ReceivePacket(PacketPtr pkt) override;

  void SendAck(EventQueueTime time_sent);

  // Records the number of bytes received so far from the current connection to
  // a metric. This is an alternative to recording cumulative_ack_ periodically,
  // and is useful for measuring the amount of data received by long-lived flows
  // over a period if time.
  void RecordBytesReceived();

 private:
  // Resets the sink's state. Will be called when the first packet in the flow
  // is received.
  void Reset();

  uint64_t cumulative_ack_;
  std::list<uint64_t> received_;

  // Each incoming packet can be tagged. This variable stores the tag of the
  // last received packet and can be used to update the metric that counts the
  // number of times tags have been changed. This signifies a change in the path
  // taken by the flow. Set to uint64_t::max if no packet has been seen yet.
  PacketTag last_seen_incoming_tag_;
  uint64_t tag_change_count_;
  metrics::UnsafeMetricHandle<uint32_t>* incoming_tag_changes_metric_;

  DISALLOW_COPY_AND_ASSIGN(TCPSink);
};

// All TCP flows share the same rtx timer, this reduces the overall number of
// events in the queue, but may cause synchronization in pathological
// situations. Usually there should be very few TCP retx timeouts.
class TCPRtxTimer : public SimComponent, public EventConsumer {
 public:
  TCPRtxTimer(const std::string& id, EventQueueTime scan_period,
              EventQueue* event_queue);

  void RegisterTCPSource(TCPSource* tcp_source);

  void HandleEvent() override;

 private:
  // How often to check if the rtx timer expired.
  const EventQueueTime scan_period_;

  // Sources to check for rtx.
  std::vector<TCPSource*> tcp_sources_;

  DISALLOW_COPY_AND_ASSIGN(TCPRtxTimer);
};

}  // namespace htsim
}  // namespace ncode

#endif
