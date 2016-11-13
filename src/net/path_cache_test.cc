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
  PathUtilsTest() : path_storage_(GenerateBraess(kDefaultBw)) {
    a_ = path_storage_.NodeFromStringOrDie("A");
    b_ = path_storage_.NodeFromStringOrDie("B");
    c_ = path_storage_.NodeFromStringOrDie("C");
    d_ = path_storage_.NodeFromStringOrDie("D");
  }

  const GraphPath* GetPath(const std::string& path_string) {
    return path_storage_.PathFromStringOrDie(path_string, 0);
  }

  GraphNodeIndex a_;
  GraphNodeIndex b_;
  GraphNodeIndex c_;
  GraphNodeIndex d_;

  PathStorage path_storage_;
  DummyConstraint dummy_constraint_;
};

static PathCacheConfig GetCacheConfig() {
  PathCacheConfig path_cache_config;
  path_cache_config.max_delay = seconds(1);
  path_cache_config.max_hops = 10;
  return path_cache_config;
}

TEST_F(PathUtilsTest, ShortestPath) {
  PathCache cache(GetCacheConfig(), &path_storage_);

  bool avoid = true;
  GraphLinkSet to_avoid;

  ASSERT_EQ(GetPath("[A->C, C->D]"),
            cache.IECache(std::make_tuple(a_, d_, 0))
                ->GetLowestDelayPath(to_avoid, &avoid));
  ASSERT_EQ(GetPath("[B->C]"), cache.IECache(std::make_tuple(b_, c_, 0))
                                   ->GetLowestDelayPath(to_avoid, &avoid));
  ASSERT_TRUE(avoid);
}

TEST_F(PathUtilsTest, KShortestPaths) {
  PathCache cache(GetCacheConfig(), &path_storage_);
  GraphLinkSet to_avoid;

  IngressEgressPathCache* ie_cache = cache.IECache(std::make_tuple(a_, d_, 0));
  ASSERT_TRUE(ie_cache->GetKLowestDelayPaths(0, to_avoid, nullptr).empty());

  std::vector<const GraphPath*> model = {GetPath("[A->C, C->D]")};
  ASSERT_EQ(model, ie_cache->GetKLowestDelayPaths(1, to_avoid, nullptr));

  model = {GetPath("[A->C, C->D]")};
  ASSERT_EQ(model, ie_cache->GetKLowestDelayPaths(1, to_avoid, nullptr));

  model = {GetPath("[A->C, C->D]"), GetPath("[A->B, B->D]")};
  ASSERT_EQ(model, ie_cache->GetKLowestDelayPaths(2, to_avoid, nullptr));

  model = {GetPath("[A->C, C->D]"), GetPath("[A->B, B->D]"),
           GetPath("[A->B, B->C, C->D]")};
  ASSERT_EQ(model, ie_cache->GetKLowestDelayPaths(3, to_avoid, nullptr));
  ASSERT_EQ(model, ie_cache->GetKLowestDelayPaths(4, to_avoid, nullptr));
}

}  // namespace net
}  // namespace ncode
