#include "pcap_consumer.h"

#include <netinet/in.h>
#include <type_traits>

#include "../common/common.h"
#include "../common/event_queue.h"
#include "../common/free_list.h"
#include "../common/logging.h"

namespace ncode {
namespace htsim {

PcapPacketGen::PcapPacketGen(
    std::unique_ptr<pcap::OfflineSourceProvider> source_provider,
    EventQueue* event_queue)
    : offline_pcap_(std::move(source_provider), this),
      max_interpacket_gap_(pcap::Timestamp::max()),
      time_shift_(pcap::Timestamp::zero()),
      prev_timestamp_(pcap::Timestamp::zero()),
      event_queue_(event_queue),
      downscale_port_(net::DevicePortNumber::Zero()) {}

void PcapPacketGen::HandleTCP(pcap::Timestamp timestamp,
                              const pcap::IPHeader& ip_header,
                              const pcap::TCPHeader& tcp_header,
                              uint16_t payload_len) {
  // All fields have to be converted to host-order.
  net::IPAddress src_address(ntohl(ip_header.ip_src.s_addr));
  net::IPAddress dst_address(ntohl(ip_header.ip_dst.s_addr));
  net::AccessLayerPort src_port(ntohs(tcp_header.th_sport));
  net::AccessLayerPort dst_port(ntohs(tcp_header.th_dport));
  size_t size = ntohs(ip_header.ip_len);
  net::FiveTuple five_tuple(src_address, dst_address, net::kProtoTCP, src_port,
                            dst_port);
  if (Ignore(five_tuple)) {
    return;
  }

  SeqNum seq_num(ntohl(tcp_header.th_seq));
  auto packet = make_unique<TCPPacket>(
      five_tuple, size, GetEventQueueTime(timestamp), seq_num);
  packet->set_id(ntohs(ip_header.ip_id));
  packet->set_flags(tcp_header.th_flags);
  packet->set_payload(payload_len);
  packet->set_ttl(ip_header.ip_ttl);
  pending_packet_ = std::move(packet);
}

void PcapPacketGen::HandleUDP(pcap::Timestamp timestamp,
                              const pcap::IPHeader& ip_header,
                              const pcap::UDPHeader& udp_header,
                              uint16_t payload_len) {
  net::IPAddress src_address(ntohl(ip_header.ip_src.s_addr));
  net::IPAddress dst_address(ntohl(ip_header.ip_dst.s_addr));
  net::AccessLayerPort src_port(ntohs(udp_header.uh_sport));
  net::AccessLayerPort dst_port(ntohs(udp_header.uh_dport));
  size_t size = ntohs(ip_header.ip_len);
  net::FiveTuple five_tuple(src_address, dst_address, net::kProtoTCP, src_port,
                            dst_port);
  if (Ignore(five_tuple)) {
    return;
  }

  auto packet = make_unique<UDPPacket>(five_tuple, size,
                                               GetEventQueueTime(timestamp));
  packet->set_id(ntohs(ip_header.ip_id));
  packet->set_payload(payload_len);
  packet->set_ttl(ip_header.ip_ttl);
  pending_packet_ = std::move(packet);
}

void PcapPacketGen::set_max_inter_packet_gap(pcap::Timestamp gap) {
  max_interpacket_gap_ = gap;
}

PacketPtr PcapPacketGen::NextPacket() {
  while (!pending_packet_) {
    if (!offline_pcap_.NextPacket()) {
      return PacketPtr();
    }
  }

  return std::move(pending_packet_);
}

EventQueueTime PcapPacketGen::GetEventQueueTime(
    pcap::Timestamp timestamp) {
  // The first packet from the trace will arrive at 0, and all other packets
  // will be offset accordingly.
  EventQueueTime now = EventQueueTime::ZeroTime();
  if (init_timestamp_ == pcap::Timestamp::max()) {
    init_timestamp_ = timestamp;
    prev_timestamp_ = timestamp;
  } else {
    CHECK(timestamp >= prev_timestamp_);
    pcap::Timestamp delta_from_last = timestamp - prev_timestamp_;
    if (delta_from_last >= max_interpacket_gap_) {
      time_shift_ += delta_from_last;
      LOG(INFO) << "Added time shift of " << delta_from_last.count()
                << " nanos";
    }
    prev_timestamp_ = timestamp;

    CHECK(timestamp >= init_timestamp_);
    pcap::Timestamp delta_from_start = timestamp - init_timestamp_;
    CHECK(delta_from_start >= time_shift_) << "ds " << delta_from_start.count()
                                           << " vs ts " << time_shift_.count();
    delta_from_start -= time_shift_;

    now = event_queue_->ToTime(delta_from_start);
  }

  return now;
}

void PcapPacketGen::EnableDownscaling(size_t n, size_t index) {
  CHECK(!downscale_rule_) << "Downscaling already enabled";
  CHECK(index < n) << "Bad index";
  CHECK(n > 1) << "N should be more than 1";

  downscale_port_ = net::DevicePortNumber(index);
  MatchRuleKey dummy_key(kWildPacketTag, kWildDevicePortNumber,
                         {net::FiveTuple::kDefaultTuple});
  downscale_rule_ = make_unique<MatchRule>(dummy_key);
  for (size_t i = 0; i < n; ++i) {
    auto action = make_unique<MatchRuleAction>(net::DevicePortNumber(i),
                                                       kWildPacketTag, 1);
    downscale_rule_->AddAction(std::move(action));
  }
}

bool PcapPacketGen::Ignore(const net::FiveTuple& five_tuple) {
  if (!downscale_rule_) {
    return false;
  }

  MatchRuleAction* action = downscale_rule_->ChooseOrNull(five_tuple);
  CHECK(action != nullptr);
  return action->output_port() != downscale_port_;
}

}  // namespace htsim
}  // namespace ncode
