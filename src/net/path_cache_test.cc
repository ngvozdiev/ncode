#include "path_cache.h"

#include "gtest/gtest.h"
#include "constraint.h"
#include "net_gen.h"

namespace ncode {
namespace net {

using namespace std::chrono;

static constexpr net::Bandwidth kDefaultBw =
    net::Bandwidth::FromBitsPerSecond(10000);

class PathUtilsTest : public ::testing::Test {
 protected:
  PathUtilsTest()
      : graph_storage_(GenerateBraess(kDefaultBw)), graph_(&graph_storage_) {
    a_ = graph_storage_.NodeFromStringOrDie("A");
    b_ = graph_storage_.NodeFromStringOrDie("B");
    c_ = graph_storage_.NodeFromStringOrDie("C");
    d_ = graph_storage_.NodeFromStringOrDie("D");
  }

  net::LinkSequence GetPath(const std::string& path_string) {
    return graph_storage_.PathFromStringOrDie(path_string, 1)->link_sequence();
  }

  GraphNodeIndex a_;
  GraphNodeIndex b_;
  GraphNodeIndex c_;
  GraphNodeIndex d_;

  PathStorage graph_storage_;
  SimpleDirectedGraph graph_;
  DummyConstraint dummy_constraint_;
};

TEST_F(PathUtilsTest, ShortestPath) {
  PathCache cache(&graph_);

  ASSERT_EQ(GetPath("[A->C, C->D]"),
            cache.IECache(a_, d_)->GetLowestDelayPath());
  ASSERT_EQ(GetPath("[B->C]"), cache.IECache(b_, c_)->GetLowestDelayPath());
}

TEST_F(PathUtilsTest, KShortestPaths) {
  PathCache cache(&graph_);

  IngressEgressPathCache* ie_cache = cache.IECache(a_, d_);
  ASSERT_TRUE(ie_cache->GetKLowestDelayPaths(0).empty());

  LOG(ERROR) << ie_cache->GetKLowestDelayPaths(1).front().ToString(
      &graph_storage_);

  std::vector<net::LinkSequence> model = {GetPath("[A->C, C->D]")};
  ASSERT_EQ(model, ie_cache->GetKLowestDelayPaths(1));

  model = {GetPath("[A->C, C->D]")};
  ASSERT_EQ(model, ie_cache->GetKLowestDelayPaths(1));

  model = {GetPath("[A->C, C->D]"), GetPath("[A->B, B->D]")};
  ASSERT_EQ(model, ie_cache->GetKLowestDelayPaths(2));

  model = {GetPath("[A->C, C->D]"), GetPath("[A->B, B->D]"),
           GetPath("[A->B, B->C, C->D]")};
  ASSERT_EQ(model, ie_cache->GetKLowestDelayPaths(3));
  ASSERT_EQ(model, ie_cache->GetKLowestDelayPaths(4));
}

}  // namespace net
}  // namespace ncode
