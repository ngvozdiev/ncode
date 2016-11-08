#include "constraint.h"
#include "gtest/gtest.h"

namespace ncode {
namespace net {
namespace {

static constexpr Bandwidth kBw = Bandwidth::FromBitsPerSecond(100);

using namespace std::chrono;

net::PBNet GetBraess() {
  PBNet net;
  AddEdgeToGraph("C", "D", milliseconds(10), kBw, &net);
  AddEdgeToGraph("D", "C", milliseconds(10), kBw, &net);
  AddEdgeToGraph("B", "D", milliseconds(8), kBw, &net);
  AddEdgeToGraph("D", "B", milliseconds(8), kBw, &net);
  AddEdgeToGraph("A", "B", milliseconds(10), kBw, &net);
  AddEdgeToGraph("B", "A", milliseconds(10), kBw, &net);
  AddEdgeToGraph("A", "C", milliseconds(5), kBw, &net);
  AddEdgeToGraph("C", "A", milliseconds(5), kBw, &net);
  AddEdgeToGraph("B", "C", milliseconds(1), kBw, &net);

  return net;
}

class ConstraintTest : public ::testing::Test {
 protected:
  ConstraintTest() : path_storage_(GetBraess()), graph_(&path_storage_) {
    a_ = path_storage_.NodeFromStringOrDie("A");
    b_ = path_storage_.NodeFromStringOrDie("B");
    c_ = path_storage_.NodeFromStringOrDie("C");
    d_ = path_storage_.NodeFromStringOrDie("D");
  }

  net::LinkSequence GetPath(const std::string& path) {
    return path_storage_.PathFromStringOrDie(path, 0ul)->link_sequence();
  }

  GraphNodeIndex a_;
  GraphNodeIndex b_;
  GraphNodeIndex c_;
  GraphNodeIndex d_;

  PathStorage path_storage_;
  SimpleDirectedGraph graph_;
};

TEST_F(ConstraintTest, Dummy) {
  DummyConstraint dummy;
  ASSERT_TRUE(dummy.PathComplies({}));
  ASSERT_EQ(GetPath("[A->C, C->D]"),
            dummy.ShortestCompliantPath(graph_, a_, d_));
  ASSERT_EQ(GetPath("[D->B, B->C]"),
            dummy.ShortestCompliantPath(graph_, d_, c_));
}

TEST_F(ConstraintTest, ConjunctionAvoidOne) {
  GraphLinkIndex bc = path_storage_.LinkOrDie("B", "C");
  Conjunction conjunction({bc}, {});

  ASSERT_EQ(GetPath("[D->C]"),
            conjunction.ShortestCompliantPath(graph_, d_, c_));
  ASSERT_EQ(GetPath("[B->A, A->C]"),
            conjunction.ShortestCompliantPath(graph_, b_, c_));
}

TEST_F(ConstraintTest, ConjunctionVisitOne) {
  GraphLinkIndex dc = path_storage_.LinkOrDie("D", "C");
  Conjunction conjunction({}, {dc});

  ASSERT_EQ(GetPath("[D->C]"),
            conjunction.ShortestCompliantPath(graph_, d_, c_));
  ASSERT_EQ(GetPath("[B->D, D->C, C->A]"),
            conjunction.ShortestCompliantPath(graph_, b_, a_));
  ASSERT_EQ(LinkSequence(), conjunction.ShortestCompliantPath(graph_, a_, b_));
}

}  // namespace
}  // namespace dfs
}  // namespace ncode
