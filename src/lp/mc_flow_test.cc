#include "../net/net_gen.h"
#include "gtest/gtest.h"
#include "mc_flow.h"

namespace ncode {
namespace lp {
namespace {

using namespace std::chrono;

class MCTest : public ::testing::Test {
 protected:
  net::LinkStorage link_storage_;
};

TEST_F(MCTest, Empty) {
  net::PBNet net;

  MaxFlowMCProblem max_flow_problem(net, &link_storage_);

  double max_flow;
  ASSERT_TRUE(max_flow_problem.GetMaxFlow(&max_flow));
  ASSERT_EQ(0, max_flow);
  ASSERT_DEATH(max_flow_problem.AddCommodity("N0", "N1"), ".*");
}

TEST_F(MCTest, UnidirectionalLink) {
  net::PBNet net = net::GenerateFullGraph(2, 10000, microseconds(10));
  net::PBGraphLink* link = net.add_links();
  link->set_src("N1");
  link->set_dst("N2");
  link->set_src_port(10);
  link->set_dst_port(102);
  link->set_bandwidth_bps(10000);

  MaxFlowMCProblem max_flow_problem(net, &link_storage_);
  max_flow_problem.AddCommodity("N0", "N2");

  double max_flow;
  ASSERT_TRUE(max_flow_problem.GetMaxFlow(&max_flow));
  ASSERT_EQ(10000.0, max_flow);
}

TEST_F(MCTest, Simple) {
  net::PBNet net = net::GenerateFullGraph(2, 10000, microseconds(10));

  double max_flow;
  MaxFlowMCProblem max_flow_problem(net, &link_storage_);
  ASSERT_TRUE(max_flow_problem.GetMaxFlow(&max_flow));
  ASSERT_EQ(0, max_flow);

  max_flow_problem.AddCommodity("N0", "N1");
  ASSERT_TRUE(max_flow_problem.GetMaxFlow(&max_flow));
  ASSERT_EQ(10000.0, max_flow);
}

TEST_F(MCTest, SimpleTwoCommodities) {
  net::PBNet net = net::GenerateFullGraph(2, 10000, microseconds(10));

  MaxFlowMCProblem max_flow_problem(net, &link_storage_);
  max_flow_problem.AddCommodity("N0", "N1");
  max_flow_problem.AddCommodity("N1", "N0");

  double max_flow;
  ASSERT_TRUE(max_flow_problem.GetMaxFlow(&max_flow));
  ASSERT_EQ(20000.0, max_flow);
}

TEST_F(MCTest, Triangle) {
  net::PBNet net = net::GenerateFullGraph(3, 10000, microseconds(10));

  MaxFlowMCProblem max_flow_problem(net, &link_storage_);
  max_flow_problem.AddCommodity("N0", "N2");

  double max_flow;
  ASSERT_TRUE(max_flow_problem.GetMaxFlow(&max_flow));
  ASSERT_EQ(20000.0, max_flow);

  max_flow_problem.AddCommodity("N1", "N2");
  ASSERT_TRUE(max_flow_problem.GetMaxFlow(&max_flow));
  ASSERT_EQ(20000.0, max_flow);
}

TEST_F(MCTest, TriangleNoFit) {
  net::PBNet net = net::GenerateFullGraph(3, 10000, microseconds(10));

  MaxFlowMCProblem max_flow_problem(net, &link_storage_);
  max_flow_problem.AddCommodity("N0", "N2", 30000);

  double max_flow;
  ASSERT_FALSE(max_flow_problem.GetMaxFlow(&max_flow));
}

TEST_F(MCTest, SimpleFeasible) {
  net::PBNet net = net::GenerateFullGraph(2, 10000, microseconds(10));
  MCProblem mc_problem(net, &link_storage_);
  ASSERT_TRUE(mc_problem.IsFeasible());

  mc_problem.AddCommodity("N0", "N1", 10000);
  ASSERT_TRUE(mc_problem.IsFeasible());

  mc_problem.AddCommodity("N1", "N0", 10001);
  ASSERT_FALSE(mc_problem.IsFeasible());
}

TEST_F(MCTest, SimpleScaleFactor) {
  net::PBNet net = net::GenerateFullGraph(2, 10000000000, microseconds(10));
  MCProblem mc_problem(net, &link_storage_);
  ASSERT_EQ(0, mc_problem.MaxCommodityScaleFactor());

  mc_problem.AddCommodity("N0", "N1");
  ASSERT_EQ(0, mc_problem.MaxCommodityScaleFactor());

  mc_problem.AddCommodity("N1", "N0", 8000);
  ASSERT_NEAR(1250000, mc_problem.MaxCommodityScaleFactor(), 0.1);
}

}  // namespace
}  // namespace lp
}  // namespace ncode
