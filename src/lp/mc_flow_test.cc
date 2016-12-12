#include "../net/net_gen.h"
#include "gtest/gtest.h"
#include "mc_flow.h"

namespace ncode {
namespace lp {
namespace {

using namespace std::chrono;

static constexpr net::Bandwidth kBw1 = net::Bandwidth::FromBitsPerSecond(10000);
static constexpr net::Bandwidth kBw2 =
    net::Bandwidth::FromBitsPerSecond(10000000000);

static net::LinkSequence GetPath(const std::string& path,
                                 const net::GraphStorage& graph_storage) {
  return graph_storage.LinkSequenceFromStringOrDie(path);
}

TEST(MCTest, UnidirectionalLink) {
  net::PBNet net = net::GenerateFullGraph(2, kBw1, microseconds(10));
  net::PBGraphLink* link = net.add_links();
  link->set_src("N1");
  link->set_dst("N2");
  link->set_src_port(10);
  link->set_dst_port(102);
  link->set_bandwidth_bps(10000);
  link->set_delay_sec(0.1);

  net::GraphStorage graph_storage(net);
  MaxFlowMCProblem max_flow_problem(&graph_storage);
  max_flow_problem.AddCommodity("N0", "N2");

  double max_flow;
  ASSERT_TRUE(max_flow_problem.GetMaxFlow(&max_flow));
  ASSERT_EQ(10000.0, max_flow);

  // The path should be N0->N1->N2
  std::vector<std::vector<FlowAndPath>> model_paths = {
      {{10000.0, GetPath("[N0->N1, N1->N2]", graph_storage)}}};
  std::vector<std::vector<FlowAndPath>> paths;
  ASSERT_TRUE(max_flow_problem.GetMaxFlow(&max_flow, &paths));
  ASSERT_EQ(10000.0, max_flow);
  ASSERT_EQ(model_paths, paths);
}

TEST(MCTest, Simple) {
  net::PBNet net = net::GenerateFullGraph(2, kBw1, microseconds(10));
  net::GraphStorage graph_storage(net);

  double max_flow;
  MaxFlowMCProblem max_flow_problem(&graph_storage);
  ASSERT_TRUE(max_flow_problem.GetMaxFlow(&max_flow));
  ASSERT_EQ(0, max_flow);

  max_flow_problem.AddCommodity("N0", "N1");
  ASSERT_TRUE(max_flow_problem.GetMaxFlow(&max_flow));
  ASSERT_EQ(10000.0, max_flow);

  std::vector<std::vector<FlowAndPath>> model_paths = {
      {{10000.0, GetPath("[N0->N1]", graph_storage)}}};
  std::vector<std::vector<FlowAndPath>> paths;
  ASSERT_TRUE(max_flow_problem.GetMaxFlow(&max_flow, &paths));
  ASSERT_EQ(10000.0, max_flow);
  ASSERT_EQ(model_paths, paths);
}

TEST(MCTest, SimpleTwoCommodities) {
  net::PBNet net = net::GenerateFullGraph(2, kBw1, microseconds(10));
  net::GraphStorage graph_storage(net);

  MaxFlowMCProblem max_flow_problem(&graph_storage);
  max_flow_problem.AddCommodity("N0", "N1");
  max_flow_problem.AddCommodity("N1", "N0");

  double max_flow;
  ASSERT_TRUE(max_flow_problem.GetMaxFlow(&max_flow));
  ASSERT_EQ(20000.0, max_flow);

  std::vector<std::vector<FlowAndPath>> model_paths = {
      {{10000.0, GetPath("[N0->N1]", graph_storage)}},
      {{10000.0, GetPath("[N1->N0]", graph_storage)}}};
  std::vector<std::vector<FlowAndPath>> paths;
  ASSERT_TRUE(max_flow_problem.GetMaxFlow(&max_flow, &paths));
  ASSERT_EQ(20000.0, max_flow);
  ASSERT_EQ(model_paths, paths);
}

TEST(MCTest, Triangle) {
  net::PBNet net = net::GenerateFullGraph(3, kBw1, microseconds(10));
  net::GraphStorage graph_storage(net);

  MaxFlowMCProblem max_flow_problem(&graph_storage);
  max_flow_problem.AddCommodity("N0", "N2");

  double max_flow;
  ASSERT_TRUE(max_flow_problem.GetMaxFlow(&max_flow));
  ASSERT_EQ(20000.0, max_flow);

  std::vector<std::vector<FlowAndPath>> model_paths = {
      {{10000.0, GetPath("[N0->N2]", graph_storage)},
       {10000.0, GetPath("[N0->N1, N1->N2]", graph_storage)}}};
  std::vector<std::vector<FlowAndPath>> paths;
  ASSERT_TRUE(max_flow_problem.GetMaxFlow(&max_flow, &paths));

  max_flow_problem.AddCommodity("N1", "N2");
  ASSERT_TRUE(max_flow_problem.GetMaxFlow(&max_flow));
  ASSERT_EQ(20000.0, max_flow);
}

TEST(MCTest, TriangleNoFit) {
  net::PBNet net = net::GenerateFullGraph(3, kBw1, microseconds(10));
  net::GraphStorage graph_storage(net);

  MaxFlowMCProblem max_flow_problem(&graph_storage);
  max_flow_problem.AddCommodity("N0", "N2", 30000);

  double max_flow;
  ASSERT_FALSE(max_flow_problem.GetMaxFlow(&max_flow));
}

TEST(MCTest, SimpleFeasible) {
  net::PBNet net = net::GenerateFullGraph(2, kBw1, microseconds(10));
  net::GraphStorage graph_storage(net);

  MCProblem mc_problem(&graph_storage);
  ASSERT_TRUE(mc_problem.IsFeasible());

  mc_problem.AddCommodity("N0", "N1", 10000);
  ASSERT_TRUE(mc_problem.IsFeasible());

  mc_problem.AddCommodity("N1", "N0", 10001);
  ASSERT_FALSE(mc_problem.IsFeasible());
}

TEST(MCTest, SimpleScaleFactor) {
  net::PBNet net = net::GenerateFullGraph(2, kBw2, microseconds(10));
  net::GraphStorage graph_storage(net);

  MCProblem mc_problem(&graph_storage);
  ASSERT_EQ(0, mc_problem.MaxCommodityScaleFactor());

  mc_problem.AddCommodity("N0", "N1");
  ASSERT_EQ(0, mc_problem.MaxCommodityScaleFactor());

  mc_problem.AddCommodity("N1", "N0", 8000);
  ASSERT_NEAR(1250000, mc_problem.MaxCommodityScaleFactor(), 0.1);
}

TEST(MCTest, SimpleIncrement) {
  net::PBNet net = net::GenerateFullGraph(2, kBw2, microseconds(10));
  net::GraphStorage graph_storage(net);

  MCProblem mc_problem(&graph_storage);
  ASSERT_EQ(0, mc_problem.MaxCommodityIncrement());

  mc_problem.AddCommodity("N0", "N1");
  ASSERT_NEAR(10000000000, mc_problem.MaxCommodityIncrement(), 0.1);
}

}  // namespace
}  // namespace lp
}  // namespace ncode
