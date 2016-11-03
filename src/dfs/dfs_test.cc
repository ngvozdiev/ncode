#include "dfs.h"

#include <chrono>
#include <memory>
#include <thread>

#include "dfs.pb.h"
#include "../common/common.h"
#include "constraint.h"
#include "gtest/gtest.h"
#include "path_utils.h"
#include "test_fixtures.h"

namespace ncode {
namespace dfs {
namespace test {

static PBDFSRequest GetDefaultRequest() {
  PBDFSRequest dfs_request;
  dfs_request.set_max_depth_hops(10);
  dfs_request.set_steps_to_check_for_stop(100000);
  dfs_request.set_max_depth_metric(2000000);
  dfs_request.set_max_duration_ms(10000);

  return dfs_request;
}

class SingleEdgeDFSFixture : public SingleEdgeFixture {
 protected:
  void RunDFS(const PBDFSRequest& request) {
    DFS dfs(request, *array_graph_, [this](const net::LinkSequence& path) {
      paths_.push_back(path);
      return true;
    });
    dfs.Search();
  }

  // A list of paths found
  vector<net::LinkSequence> paths_;
};

class BraessDFSFixture : public BraessDstD {
 protected:
  void RunDFS(const PBDFSRequest& request) {
    DFS dfs(request, *array_graph_, [this](const net::LinkSequence& path) {
      paths_.push_back(path);
      return true;
    });
    dfs.Search();
  }

  // A list of paths found
  vector<net::LinkSequence> paths_;
};

TEST_F(SingleEdgeFixture, BadSource) {
  PBDFSRequest request = GetDefaultRequest();
  request.set_src("BLAH");

  ASSERT_DEATH(DFS(request, *array_graph_, [](const net::LinkSequence&) {
                 return true;
               }), ".*");
}

TEST_F(SingleEdgeDFSFixture, SourceIsDestination) {
  PBDFSRequest request = GetDefaultRequest();
  request.set_src("B");

  // The fixture sets up a unidirectional link from A to B, the path should be
  // empty - the destination is the same as the source.
  RunDFS(request);
  ASSERT_EQ(1ul, paths_.size());

  // The only path should contain no edges.
  ASSERT_EQ(0ul, paths_.at(0).size());
}

TEST_F(SingleEdgeDFSFixture, SinglePath) {
  PBDFSRequest request = GetDefaultRequest();
  request.set_src("A");

  // A search from A should yield one path containing one edge (A->B).
  RunDFS(request);
  ASSERT_EQ(1ul, paths_.size());

  ASSERT_EQ(1ul, paths_.at(0).size());

  net::GraphLinkIndex edge_index = paths_.at(0).links().at(0);
  const net::GraphLink* edge = storage_.GetLink(edge_index);
  ASSERT_EQ("A", storage_.GetNode(edge->src())->id());
  ASSERT_EQ("B", storage_.GetNode(edge->dst())->id());
}

TEST_F(SingleEdgeDFSFixture, OneHopLimit) {
  PBDFSRequest request = GetDefaultRequest();
  request.set_src("A");
  request.set_max_depth_hops(1);

  RunDFS(request);

  // The only path is of length 1, so the DFS should find it.
  ASSERT_EQ(1ul, paths_.size());
  ASSERT_EQ(1ul, paths_.at(0).size());
}

TEST_F(SingleEdgeDFSFixture, ZeroHopLimit) {
  PBDFSRequest request = GetDefaultRequest();
  request.set_src("A");
  request.set_max_depth_hops(0);

  RunDFS(request);

  // The limit it 0 hops - there should be no paths found.
  ASSERT_EQ(0ul, paths_.size());
}

TEST_F(SingleEdgeDFSFixture, ZeroHopLimitSourceIsDestination) {
  PBDFSRequest request = GetDefaultRequest();
  request.set_src("B");
  request.set_max_depth_hops(0);

  RunDFS(request);
  ASSERT_EQ(1ul, paths_.size());

  // Even though the limit is set to 0 hops the source is the same as the
  // destination and a single empty path should be returned.
  ASSERT_EQ(0ul, paths_.at(0).size());
}

TEST_F(SingleEdgeDFSFixture, InsufficientMetricLimit) {
  PBDFSRequest request = GetDefaultRequest();
  request.set_src("A");
  request.set_max_depth_metric(0);

  RunDFS(request);

  // The metric limit is 0 - there should be no paths found since the edge by
  // default has a metric of 1.
  ASSERT_EQ(0ul, paths_.size());
}

TEST_F(BraessDFSFixture, MultiPathsMetricLimitedZero) {
  PBDFSRequest request = GetDefaultRequest();
  request.set_src("A");
  request.set_max_depth_metric(14000);

  RunDFS(request);

  // There is no path from A to D with weight less than 15000.
  ASSERT_EQ(0ul, paths_.size());
}

TEST_F(BraessDFSFixture, MultiPathsMetricLimitedOne) {
  PBDFSRequest request = GetDefaultRequest();
  request.set_src("A");
  request.set_max_depth_metric(15000);

  RunDFS(request);

  // There is one path from A to D with metric 15000
  ASSERT_EQ(1ul, paths_.size());
  ASSERT_TRUE(IsInPaths("[A->C, C->D]", graph_, paths_, &storage_));
}

TEST_F(BraessDFSFixture, MultiPathsMetricLimitedTwo) {
  PBDFSRequest request = GetDefaultRequest();
  request.set_src("A");
  request.set_max_depth_metric(18000);

  RunDFS(request);

  // There are two paths from A to D with metric <= 18000
  ASSERT_EQ(2ul, paths_.size());
  ASSERT_TRUE(IsInPaths("[A->C, C->D]", graph_, paths_, &storage_));
  ASSERT_TRUE(IsInPaths("[A->B, B->D]", graph_, paths_, &storage_));
}

TEST_F(BraessDFSFixture, MultiPathsNoMetricLimitedAll) {
  PBDFSRequest request = GetDefaultRequest();
  request.set_src("A");

  RunDFS(request);

  // The default metric depth limit should be enough to capture all paths
  ASSERT_EQ(7ul, paths_.size());
  ASSERT_TRUE(IsInPaths("[A->C, C->D]", graph_, paths_, &storage_));
  ASSERT_TRUE(IsInPaths("[A->B, B->D]", graph_, paths_, &storage_));
  ASSERT_TRUE(IsInPaths("[A->B, B->C, C->D]", graph_, paths_, &storage_));
  ASSERT_TRUE(IsInPaths("[A->B, B->A, A->C, C->D]", graph_, paths_, &storage_));
  ASSERT_TRUE(
      IsInPaths("[A->B, B->C, C->A, A->C, C->D]", graph_, paths_, &storage_));
  ASSERT_TRUE(IsInPaths("[A->C, C->A, A->B, B->D]", graph_, paths_, &storage_));
  ASSERT_TRUE(
      IsInPaths("[A->C, C->A, A->B, B->C, C->D]", graph_, paths_, &storage_));
}

TEST_F(BraessDFSFixture, MultiPathsNodeDisjoint) {
  PBDFSRequest request = GetDefaultRequest();
  request.set_src("A");
  request.set_node_disjoint(true);

  RunDFS(request);

  // The default metric depth limit should be enough to capture all three paths
  ASSERT_EQ(3ul, paths_.size());
  ASSERT_TRUE(IsInPaths("[A->C, C->D]", graph_, paths_, &storage_));
  ASSERT_TRUE(IsInPaths("[A->B, B->D]", graph_, paths_, &storage_));
  ASSERT_TRUE(IsInPaths("[A->B, B->C, C->D]", graph_, paths_, &storage_));
}

TEST_F(BraessDFSFixture, MultiPathsKAwayFromShortest) {
  PBDFSRequest request = GetDefaultRequest();
  request.set_node_disjoint(true);

  net::PathStorage storage;
  PathCache path_cache(graph_, request, &storage);

  DummyConstraint constraint;
  vector<const net::GraphPath*> paths =
      path_cache.GetPathsKHopsFromLowestDelay(constraint, 0, "A", "D", 1);

  // The set should contain A->C->D and A->B->D, both with hop count of 2.
  ASSERT_EQ(2ul, paths.size());
  for (const net::GraphPath* path : paths) {
    ASSERT_EQ(2ul, path->size());
  }

  paths = path_cache.GetPathsKHopsFromLowestDelay(constraint, 1, "A", "D", 1);
  // Should contain A->B->C->D as well.
  ASSERT_EQ(3ul, paths.size());
}

TEST_F(BraessDFSFixture, TestCallbackFalse) {
  PBDFSRequest request = GetDefaultRequest();
  request.set_src("A");

  int i = 0;
  DFS dfs(request, *array_graph_, [&i](const net::LinkSequence&) {
    if (i == 2) {
      return false;
    }

    ++i;
    return true;
  });
  dfs.Search();
  ASSERT_EQ(2, i);
}

TEST_F(LongRunningFixture, TestTimeLimit) {
  PBDFSRequest request = GetDefaultRequest();
  request.set_src("N1");
  request.set_max_duration_ms(2000);

  // Need to put in on the heap otherwise will blow up the stack.
  int i = 0;
  auto dfs_ptr =
      make_unique<DFS>(request, *array_graph_, [&i](const net::LinkSequence&) {
        ++i;
        return true;
      });

  Timer timer;
  dfs_ptr->Search();

  ASSERT_NEAR(timer.DurationMillis().count(), 2000, 100);
  ASSERT_LT(0, i);
}

TEST_F(LongRunningFixture, TestKill) {
  PBDFSRequest request = GetDefaultRequest();
  request.set_src("N1");
  request.set_max_duration_ms(10000);

  auto dfs_ptr = make_unique<DFS>(
      request, *array_graph_, [](const net::LinkSequence&) { return true; });

  Timer timer;
  std::thread dfs_thread([&dfs_ptr]() { dfs_ptr->Search(); });
  std::this_thread::sleep_for(std::chrono::milliseconds(2000));
  dfs_ptr->Terminate();
  dfs_thread.join();

  ASSERT_NEAR(timer.DurationMillis().count(), 2000, 100);
}

}  // namespace test
}  // namespace dfs
}  // namespace ncode
