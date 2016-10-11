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
  virtual ~PcapPacketGen() {}
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

  // Chooses whether to ignore a 5-tuple or not, based on downscaling.
  virtual bool Ignore(const net::FiveTuple& five_tuple) {
    Unused(five_tuple);
    return false;
  }

 private:
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
};

}  // namespace htsim
}  // namespace ncode
#endif
