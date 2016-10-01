#include "constraint.h"
#include "gtest/gtest.h"
#include "test_fixtures.h"

namespace ncode {
namespace dfs {
namespace test {

TEST_F(BraessDstD, TestBadProtobuf) {
  PBConstraint constraint;

  // No type set
  ASSERT_DEATH(CompileConstraint(constraint, &storage_), ".*");
}

TEST_F(BraessDstD, TestDummy) {
  PBConstraint constraint;
  constraint.set_type(PBConstraint::DUMMY);

  auto ag_constraint = CompileConstraint(constraint, &storage_);

  // The dummy constraint should always be valid.
  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[]")));
  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[A->B]")));
  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[D->B, B->A]")));
}

TEST_F(BraessDstD, TestVisitEdgeBadEmptyProtobuf) {
  PBConstraint constraint;
  constraint.set_type(PBConstraint::VISIT_EDGE);

  ASSERT_DEATH(CompileConstraint(constraint, &storage_), ".*");
}

TEST_F(BraessDstD, TestVisitEdgeBadDuplicateProtobuf) {
  PBConstraint constraint;
  constraint.set_type(PBConstraint::VISIT_EDGE);

  PBVisitEdgeConstraint* ve = constraint.mutable_visit_edge_constraint();
  ve->mutable_edge()->set_src("A");
  ve->mutable_edge()->set_dst("A");

  ASSERT_DEATH(CompileConstraint(constraint, &storage_), ".*");
}

TEST_F(BraessDstD, TestVisitEdgeBadNoEdgeProtobuf) {
  PBConstraint constraint;
  constraint.set_type(PBConstraint::VISIT_EDGE);

  PBVisitEdgeConstraint* ve = constraint.mutable_visit_edge_constraint();
  ve->mutable_edge()->set_src("A");
  ve->mutable_edge()->set_dst("D");

  ASSERT_DEATH(CompileConstraint(constraint, &storage_), ".*");
}

TEST_F(BraessDstD, TestVisitEdge) {
  PBConstraint constraint;
  constraint.set_type(PBConstraint::VISIT_EDGE);

  PBVisitEdgeConstraint* ve = constraint.mutable_visit_edge_constraint();
  ve->mutable_edge()->set_src("A");
  ve->mutable_edge()->set_dst("B");

  auto ag_constraint = CompileConstraint(constraint, &storage_);

  ASSERT_FALSE(ag_constraint->PathComplies(GetPath("[]")));
  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[A->B]")));
  ASSERT_FALSE(ag_constraint->PathComplies(GetPath("[D->B, B->A]")));
  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[C->A, A->B, B->A]")));
  ASSERT_FALSE(ag_constraint->PathComplies(GetPath("[A->C, C->D]")));
}

TEST_F(BraessDstD, TestAvoidEdge) {
  PBConstraint constraint;
  constraint.set_type(PBConstraint::AVOID_EDGE);

  PBAvoidEdgeConstraint* ve = constraint.mutable_avoid_edge_constraint();
  ve->mutable_edge()->set_src("A");
  ve->mutable_edge()->set_dst("B");

  auto ag_constraint = CompileConstraint(constraint, &storage_);

  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[]")));
  ASSERT_FALSE(ag_constraint->PathComplies(GetPath("[A->B]")));
  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[D->B, B->A]")));
  ASSERT_FALSE(ag_constraint->PathComplies(GetPath("[C->A, A->B, B->A]")));
  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[A->C, C->D]")));
}

TEST_F(BraessDstD, TestAvoidPathEmptyProtobuf) {
  PBConstraint constraint;
  constraint.set_type(PBConstraint::AVOID_PATH);

  ASSERT_DEATH(CompileConstraint(constraint, &storage_), ".*");
}

TEST_F(BraessDstD, TestAvoidPathEmptyProtobufTwo) {
  PBConstraint constraint;
  constraint.set_type(PBConstraint::AVOID_PATH);
  constraint.mutable_avoid_path_constraint();

  ASSERT_DEATH(CompileConstraint(constraint, &storage_), ".*");
}

TEST_F(BraessDstD, TestAvoidPathEmptyProtobufThree) {
  PBConstraint constraint;
  constraint.set_type(PBConstraint::AVOID_PATH);

  PBAvoidPathConstraint* ap = constraint.mutable_avoid_path_constraint();
  ap->mutable_path();

  ASSERT_DEATH(CompileConstraint(constraint, &storage_), ".*");
}

TEST_F(BraessDstD, TestAvoidPathBadPathEmptyEdge) {
  PBConstraint constraint;
  constraint.set_type(PBConstraint::AVOID_PATH);

  PBAvoidPathConstraint* ap = constraint.mutable_avoid_path_constraint();

  net::PBGraphLink* edge = ap->add_path();
  edge->set_src("A");
  edge->set_dst("B");

  edge = ap->add_path();

  ASSERT_DEATH(CompileConstraint(constraint, &storage_), ".*");
}

TEST_F(BraessDstD, TestAvoidPathBadPathBadEdge) {
  PBConstraint constraint;
  constraint.set_type(PBConstraint::AVOID_PATH);

  PBAvoidPathConstraint* ap = constraint.mutable_avoid_path_constraint();

  net::PBGraphLink* edge = ap->add_path();
  edge->set_src("A");
  edge->set_dst("B");

  edge = ap->add_path();
  edge->set_src("D");
  edge->set_dst("A");

  ASSERT_DEATH(CompileConstraint(constraint, &storage_), ".*");
}

TEST_F(BraessDstD, TestAvoidPathBadPathNonContiguous) {
  PBConstraint constraint;
  constraint.set_type(PBConstraint::AVOID_PATH);

  PBAvoidPathConstraint* ap = constraint.mutable_avoid_path_constraint();

  net::PBGraphLink* edge = ap->add_path();
  edge->set_src("A");
  edge->set_dst("B");

  edge = ap->add_path();
  edge->set_src("D");
  edge->set_dst("C");

  ASSERT_DEATH(CompileConstraint(constraint, &storage_), ".*");
}

TEST_F(BraessDstD, TestAvoidPath) {
  PBConstraint constraint;
  constraint.set_type(PBConstraint::AVOID_PATH);

  PBAvoidPathConstraint* ap = constraint.mutable_avoid_path_constraint();

  net::PBGraphLink* edge = ap->add_path();
  edge->set_src("A");
  edge->set_dst("B");

  edge = ap->add_path();
  edge->set_src("B");
  edge->set_dst("D");

  auto ag_constraint = CompileConstraint(constraint, &storage_);

  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[]")));
  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[A->B]")));
  ASSERT_FALSE(ag_constraint->PathComplies(GetPath("[A->B, B->D]")));
  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[A->B, B->D, D->C]")));
  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[D->B, B->A]")));
  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[C->A, A->B, B->A]")));
  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[A->C, C->D]")));
}

TEST_F(BraessDstD, TestAndEmptyProtobuf) {
  PBConstraint constraint;
  constraint.set_type(PBConstraint::AND);
  constraint.mutable_and_constraint();

  ASSERT_DEATH(CompileConstraint(constraint, &storage_), ".*");
}

TEST_F(BraessDstD, TestAndEmptyLeft) {
  PBConstraint constraint;
  constraint.set_type(PBConstraint::AND);

  PBAndConstraint* ac = constraint.mutable_and_constraint();

  PBConstraint* right_op = ac->mutable_op_two();
  right_op->set_type(PBConstraint::DUMMY);

  ASSERT_DEATH(CompileConstraint(constraint, &storage_), ".*");
}

TEST_F(BraessDstD, TestAndEmptyRight) {
  PBConstraint constraint;
  constraint.set_type(PBConstraint::AND);

  PBAndConstraint* ac = constraint.mutable_and_constraint();

  PBConstraint* left_op = ac->mutable_op_one();
  left_op->set_type(PBConstraint::DUMMY);

  ASSERT_DEATH(CompileConstraint(constraint, &storage_), ".*");
}

TEST_F(BraessDstD, TestAndDummy) {
  PBConstraint constraint;
  constraint.set_type(PBConstraint::AND);

  PBAndConstraint* ac = constraint.mutable_and_constraint();

  PBConstraint* left_op = ac->mutable_op_one();
  left_op->set_type(PBConstraint::DUMMY);

  PBConstraint* right_op = ac->mutable_op_two();
  right_op->set_type(PBConstraint::DUMMY);

  auto ag_constraint = CompileConstraint(constraint, &storage_);

  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[]")));
  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[A->B]")));
  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[A->B, B->D]")));
  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[A->B, B->D, D->C]")));
  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[D->B, B->A]")));
  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[C->A, A->B, B->A]")));
  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[A->C, C->D]")));
}

TEST_F(BraessDstD, TestAnd) {
  PBConstraint constraint;
  constraint.set_type(PBConstraint::AND);

  PBAndConstraint* ac = constraint.mutable_and_constraint();

  PBConstraint* left_op = ac->mutable_op_one();
  left_op->set_type(PBConstraint::DUMMY);

  PBConstraint* right_op = ac->mutable_op_two();
  right_op->set_type(PBConstraint::VISIT_EDGE);

  PBVisitEdgeConstraint* ve = right_op->mutable_visit_edge_constraint();
  ve->mutable_edge()->set_src("A");
  ve->mutable_edge()->set_dst("B");

  auto ag_constraint = CompileConstraint(constraint, &storage_);

  ASSERT_FALSE(ag_constraint->PathComplies(GetPath("[]")));
  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[A->B]")));
  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[A->B, B->D]")));
  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[A->B, B->D, D->C]")));
  ASSERT_FALSE(ag_constraint->PathComplies(GetPath("[D->B, B->A]")));
  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[C->A, A->B, B->A]")));
  ASSERT_FALSE(ag_constraint->PathComplies(GetPath("[A->C, C->D]")));
}

TEST_F(BraessDstD, TestAndTwo) {
  PBConstraint constraint;
  constraint.set_type(PBConstraint::AND);

  PBAndConstraint* ac = constraint.mutable_and_constraint();

  PBConstraint* left_op = ac->mutable_op_one();
  left_op->set_type(PBConstraint::VISIT_EDGE);

  PBVisitEdgeConstraint* ve = left_op->mutable_visit_edge_constraint();
  ve->mutable_edge()->set_src("D");
  ve->mutable_edge()->set_dst("C");

  PBConstraint* right_op = ac->mutable_op_two();
  right_op->set_type(PBConstraint::VISIT_EDGE);

  ve = right_op->mutable_visit_edge_constraint();
  ve->mutable_edge()->set_src("A");
  ve->mutable_edge()->set_dst("B");

  auto ag_constraint = CompileConstraint(constraint, &storage_);

  ASSERT_FALSE(ag_constraint->PathComplies(GetPath("[]")));
  ASSERT_FALSE(ag_constraint->PathComplies(GetPath("[A->B]")));
  ASSERT_FALSE(ag_constraint->PathComplies(GetPath("[A->B, B->D]")));
  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[A->B, B->D, D->C]")));
  ASSERT_FALSE(ag_constraint->PathComplies(GetPath("[D->B, B->A]")));
  ASSERT_FALSE(ag_constraint->PathComplies(GetPath("[C->A, A->B, B->A]")));
  ASSERT_FALSE(ag_constraint->PathComplies(GetPath("[A->C, C->D]")));
}

TEST_F(BraessDstD, TestOrEmptyProtobuf) {
  PBConstraint constraint;
  constraint.set_type(PBConstraint::OR);
  constraint.mutable_or_constraint();

  ASSERT_DEATH(CompileConstraint(constraint, &storage_), ".*");
}

TEST_F(BraessDstD, TestOrEmptyLeft) {
  PBConstraint constraint;
  constraint.set_type(PBConstraint::OR);

  PBOrConstraint* ac = constraint.mutable_or_constraint();

  PBConstraint* right_op = ac->mutable_op_two();
  right_op->set_type(PBConstraint::DUMMY);

  ASSERT_DEATH(CompileConstraint(constraint, &storage_), ".*");
}

TEST_F(BraessDstD, TestOrEmptyRight) {
  PBConstraint constraint;
  constraint.set_type(PBConstraint::OR);

  PBOrConstraint* ac = constraint.mutable_or_constraint();

  PBConstraint* left_op = ac->mutable_op_one();
  left_op->set_type(PBConstraint::DUMMY);

  ASSERT_DEATH(CompileConstraint(constraint, &storage_), ".*");
}

TEST_F(BraessDstD, TestOr) {
  PBConstraint constraint;
  constraint.set_type(PBConstraint::OR);

  PBOrConstraint* oc = constraint.mutable_or_constraint();

  PBConstraint* left_op = oc->mutable_op_one();
  left_op->set_type(PBConstraint::DUMMY);

  PBConstraint* right_op = oc->mutable_op_two();
  right_op->set_type(PBConstraint::VISIT_EDGE);

  PBVisitEdgeConstraint* ve = right_op->mutable_visit_edge_constraint();
  ve->mutable_edge()->set_src("A");
  ve->mutable_edge()->set_dst("B");

  auto ag_constraint = CompileConstraint(constraint, &storage_);

  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[]")));
  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[A->B]")));
  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[A->B, B->D]")));
  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[A->B, B->D, D->C]")));
  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[D->B, B->A]")));
  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[C->A, A->B, B->A]")));
  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[A->C, C->D]")));
}

TEST_F(BraessDstD, TestOrTwo) {
  PBConstraint constraint;
  constraint.set_type(PBConstraint::OR);

  PBOrConstraint* oc = constraint.mutable_or_constraint();

  PBConstraint* left_op = oc->mutable_op_one();
  left_op->set_type(PBConstraint::VISIT_EDGE);

  PBVisitEdgeConstraint* ve = left_op->mutable_visit_edge_constraint();
  ve->mutable_edge()->set_src("D");
  ve->mutable_edge()->set_dst("C");

  PBConstraint* right_op = oc->mutable_op_two();
  right_op->set_type(PBConstraint::VISIT_EDGE);

  ve = right_op->mutable_visit_edge_constraint();
  ve->mutable_edge()->set_src("A");
  ve->mutable_edge()->set_dst("B");

  auto ag_constraint = CompileConstraint(constraint, &storage_);

  ASSERT_FALSE(ag_constraint->PathComplies(GetPath("[]")));
  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[A->B]")));
  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[A->B, B->D]")));
  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[A->B, B->D, D->C]")));
  ASSERT_FALSE(ag_constraint->PathComplies(GetPath("[D->B, B->A]")));
  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[C->A, A->B, B->A]")));
  ASSERT_FALSE(ag_constraint->PathComplies(GetPath("[A->C, C->D]")));
  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[B->D, D->C]")));
}

TEST_F(BraessDstD, TestNegateEmptyProtobuf) {
  PBConstraint constraint;
  constraint.set_type(PBConstraint::NEGATE);
  constraint.mutable_negate_constraint();

  ASSERT_DEATH(CompileConstraint(constraint, &storage_), ".*");
}

TEST_F(BraessDstD, TestNegate) {
  PBConstraint constraint;
  constraint.set_type(PBConstraint::NEGATE);

  PBNegateConstraint* nc = constraint.mutable_negate_constraint();
  PBConstraint* constraint_to_negate = nc->mutable_constraint();

  constraint_to_negate->set_type(PBConstraint::VISIT_EDGE);

  PBVisitEdgeConstraint* ve =
      constraint_to_negate->mutable_visit_edge_constraint();
  ve->mutable_edge()->set_src("A");
  ve->mutable_edge()->set_dst("B");

  auto ag_constraint = CompileConstraint(constraint, &storage_);

  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[]")));
  ASSERT_FALSE(ag_constraint->PathComplies(GetPath("[A->B]")));
  ASSERT_FALSE(ag_constraint->PathComplies(GetPath("[A->B, B->D]")));
  ASSERT_FALSE(ag_constraint->PathComplies(GetPath("[A->B, B->D, D->C]")));
  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[D->B, B->A]")));
  ASSERT_FALSE(ag_constraint->PathComplies(GetPath("[C->A, A->B, B->A]")));
  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[A->C, C->D]")));
  ASSERT_TRUE(ag_constraint->PathComplies(GetPath("[B->D, D->C]")));
}

}  // namespace test
}  // namespace dfs
}  // namespace ncode
