#include "net_gen.h"

#include "gtest/gtest.h"

namespace ncode {
namespace net {

using namespace std::chrono;
static constexpr uint64_t kBandwidth = 10000;

TEST(HE, Generate) {
  PBNet net_pb = GenerateHE(kBandwidth, milliseconds(0), 1);
  // All links should be bidirectional -- 2x64 in total.
  ASSERT_EQ(112, net_pb.links_size());

  std::set<std::string> endpoints;
  for (const auto& link : net_pb.links()) {
    endpoints.insert(link.src());
    endpoints.insert(link.dst());
    ASSERT_EQ(kBandwidth, link.bandwidth_bps());

    // All links should have src and dst ports set to positive numbers.
    ASSERT_LT(0ul, link.src_port());
    ASSERT_LT(0ul, link.dst_port());
  }

  ASSERT_EQ(31ul, endpoints.size());
  ASSERT_EQ(3, net_pb.clusters_size());

  size_t total = 0;
  for (const auto& cluster : net_pb.clusters()) {
    total += cluster.nodes_size();
  }
  ASSERT_EQ(31ul, total);
}

TEST(NTT, Generate) {
  PBNet net_pb = GenerateNTT();
  ASSERT_EQ(176, net_pb.links_size());

  std::set<std::string> endpoints;
  for (const auto& link : net_pb.links()) {
    endpoints.insert(link.src());
    endpoints.insert(link.dst());

    // All links should have src and dst ports set to positive numbers.
    ASSERT_LT(0ul, link.src_port());
    ASSERT_LT(0ul, link.dst_port());
  }

  ASSERT_EQ(43ul, endpoints.size());
  size_t total = 0;
  for (const auto& cluster : net_pb.clusters()) {
    total += cluster.nodes_size();
  }
  ASSERT_EQ(43ul, total);
}

TEST(HE, GenerateDelayAdd) {
  PBNet net_pb = GenerateHE(kBandwidth, milliseconds(0), 1);
  PBNet net_pb_plus = GenerateHE(kBandwidth, milliseconds(10), 1);
  for (int i = 0; i < net_pb.links_size(); ++i) {
    const PBGraphLink& link = net_pb.links(i);
    const PBGraphLink& link_plus = net_pb_plus.links(i);
    ASSERT_NEAR(link.delay_sec(),
                std::max(0.000001, link_plus.delay_sec() - 0.010), 0.000001);
  }
}

TEST(HE, GenerateDelayMultiply) {
  PBNet net_pb = GenerateHE(kBandwidth, milliseconds(0), 1);
  PBNet net_pb_times_1000 = GenerateHE(kBandwidth, milliseconds(0), 1000);
  for (int i = 0; i < net_pb.links_size(); ++i) {
    const PBGraphLink& link = net_pb.links(i);
    const PBGraphLink& link_times_1000 = net_pb_times_1000.links(i);
    ASSERT_NEAR(link.delay_sec(),
                std::max(0.000001, link_times_1000.delay_sec() / 1000),
                0.000001);
  }
}

TEST(Ladder, NoLevels) {
  ASSERT_DEATH(GenerateLadder(0, 100000, milliseconds(10)), ".*");
}

TEST(Ladder, SingleLevel) {
  PBNet net_pb_ladder = GenerateLadder(1, 100000, milliseconds(10));
  PBNet net_pb = GenerateFullGraph(2, 100000, milliseconds(10));
  ASSERT_EQ(net_pb.DebugString(), net_pb_ladder.DebugString());
}

TEST(Ladder, MultiLevel) {
  PBNet net_pb = GenerateLadder(10, 100000, milliseconds(10));
  ASSERT_EQ((1 + 9 * 5) * 2, net_pb.links_size());

  std::set<std::string> nodes;
  for (const auto& link : net_pb.links()) {
    ASSERT_FALSE(link.src().empty());
    ASSERT_FALSE(link.dst().empty());

    nodes.emplace(link.src());
    nodes.emplace(link.dst());
  }

  ASSERT_EQ(20ul + 18ul, nodes.size());
}

}  // namespace net
}  // namespace ncode
