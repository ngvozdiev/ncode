#ifndef NCODE_NET_PATH_CACHE_H
#define NCODE_NET_PATH_CACHE_H

#include <stddef.h>
#include <chrono>
#include <map>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "../common/common.h"
#include "constraint.h"
#include "net_common.h"

namespace ncode {
namespace net {

// Caches paths between an ingress and an edgess.
class IngressEgressPathCache {
 public:
  static constexpr Delay kMaxDistance = std::chrono::seconds(1);

  IngressEgressPathCache(GraphNodeIndex src, GraphNodeIndex dst,
                         std::unique_ptr<Constraint> constraint,
                         const SimpleDirectedGraph* graph)
      : graph_(graph),
        src_(src),
        dst_(dst),
        constraint_(std::move(constraint)) {}

  IngressEgressPathCache(GraphNodeIndex src, GraphNodeIndex dst,
                         const SimpleDirectedGraph* graph)
      : graph_(graph), src_(src), dst_(dst) {
    constraint_ = make_unique<DummyConstraint>();
  }

  // Will return the lowest delay path.
  LinkSequence GetLowestDelayPath(const GraphLinkSet& to_avoid = {});

  // Will return the K lowest delay paths.
  std::vector<LinkSequence> GetKLowestDelayPaths(
      size_t k, const GraphLinkSet& to_avoid = {});

  // Will return the lowest delay path (P) and any paths that are up to
  // hop_count(P) + k hops long.
  std::vector<LinkSequence> GetPathsKHopsFromLowestDelay(
      size_t k, const GraphLinkSet& to_avoid = {});

  // Caches all paths between the source and the destination.
  const std::vector<LinkSequence>& CacheAll();

 private:
  const SimpleDirectedGraph* graph_;

  GraphNodeIndex src_;
  GraphNodeIndex dst_;

  // Optional constraint.
  std::unique_ptr<Constraint> constraint_;

  // Paths, ordered in increasing delay.
  std::vector<LinkSequence> paths_;

  DISALLOW_COPY_AND_ASSIGN(IngressEgressPathCache);
};

// An entity that will cache all possible paths between a source and
// a destination (if needed).
class PathCache {
 public:
  using ConstraintMap = std::map<std::pair<GraphNodeIndex, GraphNodeIndex>,
                                 std::unique_ptr<Constraint>>;

  // Creates a new cache.
  PathCache(const SimpleDirectedGraph* graph,
            ConstraintMap* constraint_map = nullptr);

  // Populates the cache with paths for all src-dst pairs. This can take a long
  // time and eat up a lot of memory!
  void CacheAllPaths();

  // The graph.
  const SimpleDirectedGraph* graph() { return graph_; }

  // Returns the cache between two nodes.
  IngressEgressPathCache* IECache(GraphNodeIndex src, GraphNodeIndex dst);

 private:
  const SimpleDirectedGraph* graph_;

  // Stores cached between a source and a destination.
  std::map<std::pair<GraphNodeIndex, GraphNodeIndex>,
           std::unique_ptr<IngressEgressPathCache>> ie_caches_;

  DISALLOW_COPY_AND_ASSIGN(PathCache);
};

}  // namespace dfs
}  // namespace ncode

#endif /* NCODE_NET_PATH_CACHE */
