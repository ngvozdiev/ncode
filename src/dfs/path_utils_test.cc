#include "path_utils.h"

#include "gtest/gtest.h"
#include "constraint.h"
#include "test_fixtures.h"

namespace ncode {
namespace dfs {
namespace test {

using namespace std::chrono;

static PBDFSRequest GetRequest() {
  PBDFSRequest request;
  request.set_max_depth_hops(100);
  request.set_max_depth_metric(10000000);
  request.set_node_disjoint(true);
  request.set_steps_to_check_for_stop(100000);
  request.set_max_duration_ms(10000);

  return request;
}

class PathUtilsTest : public BraessDstD {
 protected:
  DummyConstraint dummy_constraint_;
};

TEST_F(PathUtilsTest, ShortestPath) {
  PathCache cache(graph_, GetRequest(), &storage_);

  const net::GraphPath* ad_path =
      cache.GetLowestDelayPath(dummy_constraint_, "A", "D", 1);
  ASSERT_EQ(ad_path, storage_.PathFromString("[A->C, C->D]", graph_, 1));

  const net::GraphPath* bc_path =
      cache.GetLowestDelayPath(dummy_constraint_, "B", "C", 1);
  ASSERT_EQ(bc_path, storage_.PathFromString("[B->C]", graph_, 1));

  // The length of the path is precisely 15.
  ASSERT_EQ(ad_path, cache.GetLowestDelayPath(dummy_constraint_, "A", "D", 1,
                                              milliseconds(15)));

  // 14 is too low.
  ASSERT_TRUE(
      cache.GetLowestDelayPath(dummy_constraint_, "A", "D", 1, milliseconds(14))
          ->empty());
}

TEST_F(PathUtilsTest, KShortestPaths) {
  PathCache cache(graph_, GetRequest(), &storage_);

  // K is 0.
  ASSERT_TRUE(
      cache.GetKLowestDelayPaths(dummy_constraint_, 0, "A", "D", 1).empty());

  // This should only get the shortest path.
  auto paths = cache.GetKLowestDelayPaths(dummy_constraint_, 1, "A", "D", 1);
  ASSERT_EQ(1ul, paths.size());
  ASSERT_EQ(paths.front(), storage_.PathFromString("[A->C, C->D]", graph_, 1));

  paths = cache.GetKLowestDelayPaths(dummy_constraint_, 2, "A", "D", 1);
  ASSERT_EQ(2ul, paths.size());
  ASSERT_EQ(paths.front(), storage_.PathFromString("[A->C, C->D]", graph_, 1));
  ASSERT_EQ(paths.back(), storage_.PathFromString("[A->B, B->D]", graph_, 1));

  paths = cache.GetKLowestDelayPaths(dummy_constraint_, 3, "A", "D", 1);
  ASSERT_EQ(3ul, paths.size());
  ASSERT_EQ(paths.front(), storage_.PathFromString("[A->C, C->D]", graph_, 1));
  ASSERT_EQ(paths[1], storage_.PathFromString("[A->B, B->D]", graph_, 1));
  ASSERT_EQ(paths.back(),
            storage_.PathFromString("[A->B, B->C, C->D]", graph_, 1));

  ASSERT_EQ(paths,
            cache.GetKLowestDelayPaths(dummy_constraint_, 4, "A", "D", 1));
}

//// The diverse paths test needs a different topology.
class DiversePathsTest : public ::testing::Test {
 protected:
  static constexpr uint64_t kDefaultBw = 10000;

  void SetUp() override {
    net::AddBiEdgesToGraph({{"A", "B"}, {"B", "C"}, {"C", "D"}, {"E", "C"}},
                           milliseconds(1), kDefaultBw, &graph_);
    net::AddBiEdgesToGraph({{"A", "F"}, {"F", "E"}, {"E", "D"}},
                           milliseconds(5), kDefaultBw, &graph_);
    net::AddBiEdgesToGraph({{"F", "B"}}, milliseconds(20), kDefaultBw, &graph_);
  }

  net::PBNet graph_;
  net::PathStorage storage_;
  DummyConstraint dummy_constraint_;
};

TEST_F(DiversePathsTest, KDiversePaths) {
  PathCache cache(graph_, GetRequest(), &storage_);

  auto p1 = storage_.PathFromString("[A->B, B->C, C->D]", graph_, 1);
  auto p2 = storage_.PathFromString("[A->F, F->E, E->D]", graph_, 1);
  auto p3 = storage_.PathFromString("[A->B, B->C, C->E, E->D]", graph_, 1);
  auto p4 = storage_.PathFromString("[A->F, F->E, E->C, C->D]", graph_, 1);

  ASSERT_TRUE(
      cache.GetKDiversePaths(dummy_constraint_, 0, "A", "D", 1).empty());

  auto paths = cache.GetKDiversePaths(dummy_constraint_, 1, "A", "D", 1);
  ASSERT_EQ(1ul, paths.size());
  ASSERT_EQ(paths.front(), p1);

  paths = cache.GetKDiversePaths(dummy_constraint_, 2, "A", "D", 1);
  ASSERT_EQ(2ul, paths.size());

  ASSERT_EQ(paths.front(), p1);
  ASSERT_EQ(paths.back(), p2);

  paths = cache.GetKDiversePaths(dummy_constraint_, 3, "A", "D", 1);
  ASSERT_EQ(3ul, paths.size());
  ASSERT_EQ(paths.front(), p1);
  ASSERT_EQ(paths[1], p3);
  ASSERT_EQ(paths.back(), p2);

  paths = cache.GetKDiversePaths(dummy_constraint_, 4, "A", "D", 1);
  ASSERT_EQ(4ul, paths.size());
  ASSERT_EQ(paths.front(), p1);
  ASSERT_EQ(paths[1], p3);
  ASSERT_EQ(paths[2], p4);
  ASSERT_EQ(paths.back(), p2);

  paths = cache.GetKDiversePaths(dummy_constraint_, 100, "A", "D", 1);
  ASSERT_EQ(8ul, paths.size());
}

}  // namespace test
}  // namespace dfs
}  // namespace ncode
