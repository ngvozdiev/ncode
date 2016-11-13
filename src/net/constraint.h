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

  // Returns the shortest compliant path that avoids a set of links if possible.
  // The second return value indicates if the path avoids the links in
  // 'to_avoid'. If there is no compliant path (even one that goes through links
  // in 'to_avoid') the second value will always be true, and the returned path
  // will be empty.
  virtual net::LinkSequence ShortestCompliantPath(
      const SimpleDirectedGraph& graph, const GraphLinkSet& to_avoid,
      GraphNodeIndex src, GraphNodeIndex dst, bool* avoids) const = 0;

  virtual std::string ToString(const net::GraphStorage* storage) const = 0;

 protected:
  Constraint() {}

  DISALLOW_COPY_AND_ASSIGN(Constraint);
};

// A conjunctions is a list of links that should be visited in order, and a set
// of links that should not be visited.
class Conjunction : public Constraint {
 public:
  Conjunction(const GraphLinkSet& to_exclude,
              const std::vector<GraphLinkIndex>& to_visit);

  bool PathComplies(const net::LinkSequence& link_sequence) const override;

  net::LinkSequence ShortestCompliantPath(const SimpleDirectedGraph& graph,
                                          const GraphLinkSet& to_avoid,
                                          GraphNodeIndex src,
                                          GraphNodeIndex dst,
                                          bool* avoids) const override;

  std::string ToString(const net::GraphStorage* storage) const override;

 private:
  static void AddFromPath(const SimpleDirectedGraph& graph,
                          const LinkSequence& path, Links* out,
                          GraphNodeSet* nodes);

  GraphLinkSet to_exclude_;
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
                                          const GraphLinkSet& to_avoid,
                                          GraphNodeIndex src,
                                          GraphNodeIndex dst,
                                          bool* avoids) const override;

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
                                          const GraphLinkSet& to_avoid,
                                          GraphNodeIndex src,
                                          GraphNodeIndex dst,
                                          bool* avoids) const override;

  std::string ToString(const net::GraphStorage* storage) const override;
};

}  // namespace dfs
}  // namespace ncode

#endif /* NCODE_CONSTRAINT_H */
