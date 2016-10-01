#ifndef HTSIM_PCAP_CONSUMER_H
#define HTSIM_PCAP_CONSUMER_H

#include <stddef.h>
#include <chrono>
#include <cstdint>
#include <memory>

#include "../net/net_common.h"
#include "../net/pcap.h"
#include "bulk_gen.h"
#include "match.h"
#include "packet.h"

namespace ncode {
namespace htsim {

// A class that reads packets from a .pcap file trace and hands them off to a
// PacketHandler. The packets will be clocked off the timestamps in the file.
class PcapPacketGen : public BulkPacketSource, public pcap::PacketHandler {
 public:
  PcapPacketGen(std::unique_ptr<pcap::OfflineSourceProvider> source_provider,
                EventQueue* event_queue);

  void HandleTCP(pcap::Timestamp timestamp, const pcap::IPHeader& ip_header,
                 const pcap::TCPHeader& tcp_header,
                 uint16_t payload_len) override;

  void HandleUDP(pcap::Timestamp timestamp, const pcap::IPHeader& ip_header,
                 const pcap::UDPHeader& udp_header,
                 uint16_t payload_len) override;

  PacketPtr NextPacket() override;

  void set_max_inter_packet_gap(pcap::Timestamp gap);

  // Enables downscaling -- only 1/n of the flows in the trace will be seen by
  // the generator. The second argument determines which 1/n of the flows will
  // be seen. For example if you have a big trace that you want to split in 10
  // pieces you can have 10 generators for the same trace file on which you do
  // EnableDownscaling(10, 0), EnableDownscaling(10, 1) ...
  // EnableDownscaling(10, 9). This function can only be called once.
  void EnableDownscaling(size_t n, size_t index);

 private:
  // Chooses whether to ignore a 5-tuple or not, based on downscaling.
  bool Ignore(const net::FiveTuple& five_tuple);

  // Converts from pcap timestamps to event queue time.
  EventQueueTime GetEventQueueTime(pcap::Timestamp timestamp);

  // Reads from the .pcap file.
  pcap::OfflinePcap offline_pcap_;

  // The currently pending packet.
  PacketPtr pending_packet_;

  // Initial timestamp.
  pcap::Timestamp init_timestamp_ = pcap::Timestamp::max();

  // Max gap between packets in the trace. If two packets are more than this
  // apart the second and all subsequent packets will be shifted in time to
  // close the gap.
  pcap::Timestamp max_interpacket_gap_;

  // Gaps in the timestamps cause time shift to accumulate.
  pcap::Timestamp time_shift_;

  // Previous timestamp. Used when calculating time shift.
  pcap::Timestamp prev_timestamp_;

  // The event queue. Used when transferring timestamps.
  EventQueue* event_queue_;

  // Downscaling is done by relying on a dummy rule to split the traffic for us.
  // The rule will have a number of output "ports" with equal probability,
  // defined when enabling downsampling. The rule will choose an action for each
  // packet that comes in. Only one action (the one whose "port" index is
  // downscale_bucket_) will result in packets being kept. All other actions
  // will discard packets.
  net::DevicePortNumber downscale_port_;
  std::unique_ptr<MatchRule> downscale_rule_;
};

}  // namespace htsim
}  // namespace ncode
#endif
