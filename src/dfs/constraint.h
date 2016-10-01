#ifndef NCODE_CONSTRAINT_H
#define NCODE_CONSTRAINT_H

#include <cstdbool>
#include <iostream>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "dfs.pb.h"
#include "../common/common.h"
#include "../net/net_common.h"

namespace ncode {
namespace dfs {

// A constraint that can be used by DFS instances to check a graph path for
// compliance.
class Constraint {
 public:
  virtual ~Constraint() {}

  // Whether or not a path complies.
  virtual bool PathComplies(const net::LinkSequence& link_sequence) const = 0;

  virtual std::string ToString() const = 0;

 protected:
  Constraint() {}

  DISALLOW_COPY_AND_ASSIGN(Constraint);
};

std::unique_ptr<Constraint> CompileConstraint(const PBConstraint& constraint,
                                              net::PathStorage* storage);

// A dummy constraint that considers any path compliant.
class DummyConstraint : public Constraint {
 public:
  DummyConstraint() {}

  bool PathComplies(const net::LinkSequence& link_sequence) const override {
    Unused(link_sequence);
    return true;
  }

  std::string ToString() const override { return "[DUMMY]"; }
};

// A function pointer that takes a path and two constraints and returns a bool.
typedef bool (*binary_bool_op)(const net::LinkSequence& link_sequence,
                               const Constraint& left, const Constraint& right);

// Ideally there will be a common base class and a subclass for OR and AND, but
// constructing those classes can fail. Since we don't want to throw exceptions
// in constructors and static functions cannot be virtual we instead templatize
// the base class and pass function pointers for AND and OR as template
// arguments later.
// TODO(gvozdiev): Figure out if this will get properly inlined when compiled
template <binary_bool_op op>
class BinaryConstraint : public Constraint {
 public:
  BinaryConstraint(const PBConstraint& left, const PBConstraint& right,
                   net::PathStorage* storage)
      : left_constraint_(CompileConstraint(left, storage)),
        right_constraint_(CompileConstraint(right, storage)) {}

  BinaryConstraint(std::unique_ptr<Constraint> left,
                   std::unique_ptr<Constraint> right)
      : left_constraint_(std::move(left)),
        right_constraint_(std::move(right)) {}

  bool PathComplies(const net::LinkSequence& link_sequence) const override {
    return op(link_sequence, *left_constraint_, *right_constraint_);
  }

  std::string ToString() const override {
    return "[" + left_constraint_->ToString() + " OP " +
           right_constraint_->ToString() + "]";
  }

 private:
  const std::unique_ptr<Constraint> left_constraint_;
  const std::unique_ptr<Constraint> right_constraint_;
};

// A function that performs an AND
static bool AndOp(const net::LinkSequence& link_sequence,
                  const Constraint& left, const Constraint& right) {
  return left.PathComplies(link_sequence) && right.PathComplies(link_sequence);
}

// A function that performs an OR
static bool OrOp(const net::LinkSequence& link_sequence, const Constraint& left,
                 const Constraint& right) {
  return left.PathComplies(link_sequence) || right.PathComplies(link_sequence);
}

typedef BinaryConstraint<AndOp> AndConstraint;
typedef BinaryConstraint<OrOp> OrConstraint;

// A constraint that will exclude a path if it exactly matches the path in the
// constraint.
class AvoidPathConstraint : public Constraint {
 public:
  AvoidPathConstraint(const PBAvoidPathConstraint& avoid_path_constraint,
                      net::PathStorage* storage);

  bool PathComplies(const net::LinkSequence& link_sequence) const override;

  std::string ToString() const override;

 private:
  const net::GraphPath* to_avoid_;
};

// A constraint that will exclude all paths that do not go across an edge.
class VisitEdgeConstraint : public Constraint {
 public:
  VisitEdgeConstraint(const PBVisitEdgeConstraint& visit_edge_constraint,
                      net::LinkStorage* storage);

  bool PathComplies(const net::LinkSequence& link_sequence) const override;

  std::string ToString() const override;

 private:
  const net::GraphLink* edge_;
};

// A constraint that will exclude all paths that go across an edge.
class AvoidEdgeConstraint : public Constraint {
 public:
  AvoidEdgeConstraint(const PBAvoidEdgeConstraint& avoid_edge_constraint,
                      net::LinkStorage* storage);

  AvoidEdgeConstraint(const net::GraphLink* edge) : edge_(edge) {}

  bool PathComplies(const net::LinkSequence& link_sequence) const override;

  std::string ToString() const override;

 private:
  const net::GraphLink* edge_;
};

// An optimized version of the AvoidEdgeConstraint for multiple edges.
class AvoidEdgesConstraint : public Constraint {
 public:
  AvoidEdgesConstraint(const std::vector<const net::GraphLink*>& edges);

  bool PathComplies(const net::LinkSequence& link_sequence) const override;

  std::string ToString() const override {
    return "[BULK_AVOID " + std::to_string(edges_to_avoid_.size()) + "]";
  }

 private:
  // Sorted list of edges to avoid.
  std::vector<const net::GraphLink*> edges_to_avoid_;
};

// A constraint that can be used to negate another constraint.
class NegateConstraint : public Constraint {
 public:
  NegateConstraint(const PBNegateConstraint& negate_constraint,
                   net::PathStorage* storage)
      : constraint_(
            CompileConstraint(negate_constraint.constraint(), storage)) {}

  bool PathComplies(const net::LinkSequence& path) const override {
    return !constraint_->PathComplies(path);
  }

  std::string ToString() const override {
    return "[NEGATE " + constraint_->ToString() + "]";
  }

 private:
  const std::unique_ptr<Constraint> constraint_;
};

}  // namespace dfs
}  // namespace ncode

#endif /* NCODE_CONSTRAINT_H */
