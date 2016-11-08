#ifndef NCODE_CONSTRAINT_H
#define NCODE_CONSTRAINT_H

#include <cstdbool>
#include <iostream>
#include <memory>
#include <type_traits>
#include <vector>

#include "../common/common.h"
#include "algorithm.h"
#include "net_common.h"

namespace ncode {
namespace net {

// A constraint that can be used by algorithm instances to check for compliance.
class Constraint {
 public:
  virtual ~Constraint() {}

  // Whether or not a path complies.
  virtual bool PathComplies(const net::LinkSequence& link_sequence) const = 0;

  // Returns the shortest compliant path under a given view.
  virtual net::LinkSequence ShortestCompliantPath(
      const SimpleDirectedGraph& graph, GraphNodeIndex src,
      GraphNodeIndex dst) const = 0;

  virtual std::string ToString(const net::GraphStorage* storage) const = 0;

 protected:
  Constraint() {}

  DISALLOW_COPY_AND_ASSIGN(Constraint);
};

// A conjunctions is a list of links that should be visited in order, and a set
// of links that should not be visited.
class Conjunction : public Constraint {
 public:
  Conjunction(const GraphLinkSet& to_avoid,
              const std::vector<GraphLinkIndex>& to_visit);

  bool PathComplies(const net::LinkSequence& link_sequence) const override;

  net::LinkSequence ShortestCompliantPath(const SimpleDirectedGraph& graph,
                                          GraphNodeIndex src,
                                          GraphNodeIndex dst) const override;

  std::string ToString(const net::GraphStorage* storage) const override;

 private:
  static void AddFromPath(const SimpleDirectedGraph& graph,
                          const LinkSequence& path, Links* out,
                          GraphNodeSet* nodes);

  GraphLinkSet to_avoid_;
  std::vector<GraphLinkIndex> to_visit_;
};

// A disjunction is a series of conjunctions.
class Disjunction : public Constraint {
 public:
  Disjunction() {}

  void AddConjunction(std::unique_ptr<Conjunction> conjunction) {
    conjunctions_.emplace_back(std::move(conjunction));
  }

  bool PathComplies(const net::LinkSequence& link_sequence) const override;

  net::LinkSequence ShortestCompliantPath(const SimpleDirectedGraph& graph,
                                          GraphNodeIndex src,
                                          GraphNodeIndex dst) const override;

  std::string ToString(const net::GraphStorage* storage) const override;

 private:
  std::vector<std::unique_ptr<Conjunction>> conjunctions_;
};

// A dummy constraint that considers any path compliant, and returns the
// shortest path between the source and the destination.
class DummyConstraint : public Constraint {
 public:
  DummyConstraint() {}

  bool PathComplies(const net::LinkSequence& link_sequence) const override;

  net::LinkSequence ShortestCompliantPath(const SimpleDirectedGraph& graph,
                                          GraphNodeIndex src,
                                          GraphNodeIndex dst) const override;

  std::string ToString(const net::GraphStorage* storage) const override;
};

}  // namespace dfs
}  // namespace ncode

#endif /* NCODE_CONSTRAINT_H */
