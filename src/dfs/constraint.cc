#include "constraint.h"

#include <google/protobuf/repeated_field.h>
#include <algorithm>

#include "../common/logging.h"
#include "../common/substitute.h"

namespace ncode {
namespace dfs {

std::unique_ptr<Constraint> CompileConstraint(const PBConstraint& constraint,
                                              net::PathStorage* storage) {
  auto type = constraint.type();
  switch (type) {
    case PBConstraint::DUMMY: {
      return make_unique<DummyConstraint>();
    }
    case PBConstraint::AND: {
      return make_unique<AndConstraint>(constraint.and_constraint().op_one(),
                                        constraint.and_constraint().op_two(),
                                        storage);
    }
    case PBConstraint::OR: {
      return make_unique<OrConstraint>(constraint.or_constraint().op_one(),
                                       constraint.or_constraint().op_two(),
                                       storage);
    }
    case PBConstraint::AVOID_PATH: {
      return make_unique<AvoidPathConstraint>(
          constraint.avoid_path_constraint(), storage);
    }
    case PBConstraint::VISIT_EDGE: {
      return make_unique<VisitEdgeConstraint>(
          constraint.visit_edge_constraint(), storage);
    }
    case PBConstraint::NEGATE: {
      return make_unique<NegateConstraint>(constraint.negate_constraint(),
                                           storage);
    }
    case PBConstraint::AVOID_EDGE: {
      return make_unique<AvoidEdgeConstraint>(
          constraint.avoid_edge_constraint(), storage);
    }

    default:
      LOG(FATAL) << "Don't know what to do with constraint";
      return std::unique_ptr<Constraint>();
  }
}

AvoidPathConstraint::AvoidPathConstraint(
    const PBAvoidPathConstraint& avoid_path_constraint,
    net::PathStorage* storage)
    : to_avoid_(storage->PathFromProtobuf(avoid_path_constraint.path(), 0)) {
  CHECK(avoid_path_constraint.path_size())
      << "Path cannot be empty in constraint";
}

bool AvoidPathConstraint::PathComplies(
    const net::LinkSequence& link_sequence) const {
  return link_sequence.links() != to_avoid_->link_sequence().links();
}

std::string AvoidPathConstraint::ToString(
    const net::LinkStorage* storage) const {
  Unused(storage);
  return "[AVOID_PATH " + to_avoid_->ToString() + "]";
}

VisitEdgeConstraint::VisitEdgeConstraint(
    const PBVisitEdgeConstraint& visit_edge_constraint,
    net::LinkStorage* storage)
    : edge_(storage->LinkFromProtobuf(visit_edge_constraint.edge())) {}

bool VisitEdgeConstraint::PathComplies(
    const net::LinkSequence& link_sequence) const {
  for (net::GraphLinkIndex edge_in_path : link_sequence.links()) {
    if (edge_in_path == edge_) {
      return true;
    }
  }

  return false;
}

std::string VisitEdgeConstraint::ToString(
    const net::LinkStorage* storage) const {
  return Substitute("[VISIT_EDGE $0 ($1)]", storage->GetLink(edge_)->ToString(),
                    static_cast<const void*>(&edge_));
}

AvoidEdgeConstraint::AvoidEdgeConstraint(
    const PBAvoidEdgeConstraint& avoid_edge_constraint,
    net::LinkStorage* storage)
    : edge_(storage->LinkFromProtobuf(avoid_edge_constraint.edge())) {}

bool AvoidEdgeConstraint::PathComplies(
    const net::LinkSequence& link_sequence) const {
  return !link_sequence.Contains(edge_);
}

std::string AvoidEdgeConstraint::ToString(
    const net::LinkStorage* storage) const {
  return Substitute("[AVOID_EDGE $0 ($1)]", storage->GetLink(edge_)->ToString(),
                    static_cast<const void*>(&edge_));
}

AvoidEdgesConstraint::AvoidEdgesConstraint(
    const std::vector<net::GraphLinkIndex>& edges)
    : edges_to_avoid_(edges) {
  std::sort(edges_to_avoid_.begin(), edges_to_avoid_.end());
}

bool AvoidEdgesConstraint::PathComplies(
    const net::LinkSequence& link_sequence) const {
  for (net::GraphLinkIndex link : link_sequence.links()) {
    if (std::binary_search(edges_to_avoid_.begin(), edges_to_avoid_.end(),
                           link)) {
      return false;
    }
  }

  return true;
}

}  // namespace dfs
}  // namespace ncode
