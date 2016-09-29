#ifndef NCODE_NET_GEN_H
#define NCODE_NET_GEN_H

#include "net_common.h"

namespace ncode {
namespace net {

// Generates a topology similar to HE's backbone. The topology will have 31
// devices and 56 bidirectional links. All links in the network will have a
// delay of max(1, d * delay_multiply + delay_add) where d is the speed of light
// in fiber delay.
PBNet GenerateHE(
    const PBGraphLink& link_template,
    std::chrono::microseconds delay_add = std::chrono::microseconds(0),
    double delay_multiply = 1.0);

// Generates a full graph of a given size. Each node will be named Ni for i in
// [0, size).
net::PBNet GenerateFullGraph(uint32_t size, uint64_t rate_bps,
                             std::chrono::microseconds delay);

// Generates a ladder-like topology. All links have the same rate and delay.
// Nodes will be named Ni for i in [0, ...]. Odd nodes will be on one side
// of the ladder, even nodes will be on the other. If the central_delays
// argument is not empty it should contain as many elements as levels. Each
// element will be the delay of the middle connecting link for the respective
// level. With 1 level this is a line, 2 levels a hexagon, 3 levels two hexagons
// attached etc.
net::PBNet GenerateLadder(
    size_t levels, uint64_t rate_bps, std::chrono::microseconds delay,
    const std::vector<std::chrono::microseconds>& central_delays = {});

}  // namespace net
}  // namespace ncode
#endif
