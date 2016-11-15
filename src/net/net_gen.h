#ifndef NCODE_NET_GEN_H
#define NCODE_NET_GEN_H

#include <chrono>
#include <cstdint>
#include <random>
#include <vector>
#include "../net/net_common.h"

namespace ncode {
namespace net {

// Generates a topology similar to HE's backbone. The topology will have 31
// devices and 56 bidirectional links. All links in the network will have a
// delay of max(1, d * delay_multiply + delay_add) where d is the speed of light
// in fiber delay.
PBNet GenerateHE(Bandwidth bw, Delay delay_add = Delay(0),
                 double delay_multiply = 1.0);

// Generates a topology similar to NTT's backbone. Unlike HE above this function
// will also populate real-world bandiwdth values.
PBNet GenerateNTT(Delay delay_add = Delay(0), double delay_multiply = 1.0,
                  Bandwidth bw = Bandwidth::FromBitsPerSecond(0),
                  double bw_multiply = 1.0);

// Generates a full graph of a given size. Each node will be named Ni for i in
// [0, size).
PBNet GenerateFullGraph(uint32_t size, Bandwidth bw, Delay delay);

// Generates a ladder-like topology. All links have the same rate and delay.
// Nodes will be named Ni for i in [0, ...]. Odd nodes will be on one side
// of the ladder, even nodes will be on the other. If the central_delays
// argument is not empty it should contain as many elements as levels. Each
// element will be the delay of the middle connecting link for the respective
// level. With 1 level this is a line, 2 levels a hexagon, 3 levels two hexagons
// attached etc.
PBNet GenerateLadder(size_t levels, Bandwidth rate_bps, Delay delay,
                     const std::vector<Delay>& central_delays = {});

// Generates a random graph with N nodes. Each of the n * (n - 1) edges has a
// uniform probability of edge_prob of being part of the graph. If this is set
// to 1.0 the graph will be a full graph. Each edge's delay and bandwidth will
// be picked at uniform from the given ranges. Nodes will be named Ni for i in
// [0, N).
PBNet GenerateRandom(size_t n, double edge_prob, Delay delay_min,
                     Delay delay_max, Bandwidth bw_bps_min,
                     Bandwidth bw_bps_max, std::mt19937* generator);

// A simple Braess paradox-like topology used for testing.
PBNet GenerateBraess(Bandwidth bw);

// Generates one of the Rocketfuel topologies.
enum RocketfuelTopology { AS1221, AS1239, AS1755, AS3257, AS3967, AS6461 };
PBNet GenerateRocketfuel(RocketfuelTopology topology, Bandwidth bw,
                         Delay delay_add = Delay(0),
                         double delay_multiply = 1.0);

}  // namespace net
}  // namespace ncode
#endif
