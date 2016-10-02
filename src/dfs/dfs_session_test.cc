#include "dfs_session.h"

#include <chrono>
#include <string>
#include <vector>

#include "dfs.pb.h"
#include "net.pb.h"
#include "../net/net_common.h"
#include "../net/net_gen.h"
#include "gtest/gtest.h"
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

class SmallFullGraphFixture : public ::testing::Test {
 protected:
  SmallFullGraphFixture() {}

  void SetUp() override {
    graph_ = net::GenerateFullGraph(3, 1000, std::chrono::milliseconds(1));
    constraint_.set_type(PBConstraint::DUMMY);

    session_ = make_unique<DFSSession>(graph_, "N0", constraint_, 1, &storage_);
  }

  net::PathStorage storage_;
  net::PBNet graph_;
  PBConstraint constraint_;
  std::unique_ptr<DFSSession> session_;
};

TEST_F(SmallFullGraphFixture, TerminateBeforeRequest) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";

  PBDFSRequest request = GetDefaultRequest();
  request.set_src("N1");

  session_->TerminateSession();
  ASSERT_DEATH(session_->ProcessRequestOrThrow(request), ".*");
}

TEST_F(SmallFullGraphFixture, SingleRequest) {
  PBDFSRequest request = GetDefaultRequest();
  request.set_src("N1");

  auto paths = session_->ProcessRequestOrThrow(request);

  ASSERT_EQ(3ul, paths.size());
  ASSERT_TRUE(IsInPaths("[N1->N0]", graph_, paths, 1, &storage_));
  ASSERT_FALSE(IsInPaths("[N1->N0]", graph_, paths, 0, &storage_));
  ASSERT_TRUE(IsInPaths("[N1->N2, N2->N0]", graph_, paths, 1, &storage_));
  ASSERT_TRUE(
      IsInPaths("[N1->N2, N2->N1, N1->N0]", graph_, paths, 1, &storage_));
}

TEST_F(SmallFullGraphFixture, DoubleRequest) {
  PBDFSRequest request = GetDefaultRequest();
  request.set_src("N1");

  auto paths_one = session_->ProcessRequestOrThrow(request);
  auto paths_two = session_->ProcessRequestOrThrow(request);

  ASSERT_EQ(paths_one, paths_two);
}

TEST_F(SmallFullGraphFixture, SingleRequestPathCountLimit) {
  PBDFSRequest request = GetDefaultRequest();
  request.set_src("N1");

  auto paths = session_->ProcessRequestOrThrow(request, 1);

  ASSERT_EQ(1ul, paths.size());
  ASSERT_TRUE(IsInPaths("[N1->N0]", graph_, paths, 1, &storage_));
}

TEST_F(SmallFullGraphFixture, TwoRequests) {
  PBDFSRequest request_one = GetDefaultRequest();
  request_one.set_src("N1");

  PBDFSRequest request_two = GetDefaultRequest();
  request_two.set_src("N2");

  auto paths_one = session_->ProcessRequestOrThrow(request_one);
  auto paths_two = session_->ProcessRequestOrThrow(request_two);

  ASSERT_EQ(3ul, paths_one.size());
  ASSERT_EQ(3ul, paths_two.size());
  ASSERT_TRUE(IsInPaths("[N1->N0]", graph_, paths_one, 1, &storage_));
  ASSERT_TRUE(IsInPaths("[N1->N2, N2->N0]", graph_, paths_one, 1, &storage_));
  ASSERT_TRUE(
      IsInPaths("[N1->N2, N2->N1, N1->N0]", graph_, paths_one, 1, &storage_));
  ASSERT_TRUE(IsInPaths("[N2->N0]", graph_, paths_two, 1, &storage_));
  ASSERT_TRUE(IsInPaths("[N2->N1, N1->N0]", graph_, paths_two, 1, &storage_));
  ASSERT_TRUE(
      IsInPaths("[N2->N1, N1->N2, N2->N0]", graph_, paths_two, 1, &storage_));
}

TEST_F(LongRunningFixture, TestForceTerminate) {
  PBDFSRequest request = GetDefaultRequest();
  request.set_src("N1");
  request.set_max_duration_ms(10000);

  PBConstraint dummy_constraint;
  dummy_constraint.set_type(PBConstraint::DUMMY);

  DFSSession session(graph_, "N2", dummy_constraint, 1, &storage_);
  std::thread thread(
      [&session, &request] { session.ProcessRequestOrThrow(request); });

  Timer timer;
  std::this_thread::sleep_for(std::chrono::milliseconds(2000));

  session.TerminateSession();
  thread.join();
  ASSERT_NEAR(timer.DurationMillis().count(), 2000, 100);
}

// A simple fixture that causes the DFS to avoid N1->N0
class SmallFullGraphFixtureWithConstraint : public SmallFullGraphFixture {
  void SetUp() override {
    constraint_.set_type(PBConstraint::NEGATE);
    PBNegateConstraint* nc = constraint_.mutable_negate_constraint();

    PBConstraint* constraint_to_negate = nc->mutable_constraint();
    constraint_to_negate->set_type(PBConstraint::VISIT_EDGE);

    PBVisitEdgeConstraint* vc =
        constraint_to_negate->mutable_visit_edge_constraint();
    vc->mutable_edge()->set_src("N1");
    vc->mutable_edge()->set_dst("N0");

    graph_ = net::GenerateFullGraph(3, 1000, std::chrono::milliseconds(1));
    session_ = make_unique<DFSSession>(graph_, "N0", constraint_, 1, &storage_);
  }
};

TEST_F(SmallFullGraphFixtureWithConstraint, SessionWideConstraint) {
  PBDFSRequest request = GetDefaultRequest();
  request.set_src("N1");

  auto paths = session_->ProcessRequestOrThrow(request);
  ASSERT_EQ(1ul, paths.size());
  ASSERT_TRUE(IsInPaths("[N1->N2, N2->N0]", graph_, paths, 1, &storage_));
}

TEST_F(SmallFullGraphFixture, RequestConstraint) {
  PBDFSRequest request = GetDefaultRequest();
  request.set_src("N1");

  PBConstraint constraint;
  constraint.set_type(PBConstraint::VISIT_EDGE);

  PBVisitEdgeConstraint* vc = constraint.mutable_visit_edge_constraint();
  vc->mutable_edge()->set_src("N1");
  vc->mutable_edge()->set_dst("N0");

  auto paths = session_->ProcessRequestOrThrow(request, 100, constraint);

  ASSERT_EQ(2ul, paths.size());
  ASSERT_TRUE(IsInPaths("[N1->N0]", graph_, paths, 1, &storage_));
  ASSERT_TRUE(
      IsInPaths("[N1->N2, N2->N1, N1->N0]", graph_, paths, 1, &storage_));
}

}  // namespace test
}  // namespace dfs
}  // namespace ncode

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
