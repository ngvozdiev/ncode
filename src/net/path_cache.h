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

using IngressEgressKey = std::tuple<GraphNodeIndex, GraphNodeIndex, uint64_t>;

struct PathCacheConfig {
  Delay max_delay;
  size_t max_hops;
};

// Caches paths between an ingress and an edgess.
class IngressEgressPathCache {
 public:
  // Will return the lowest delay path.
  const GraphPath* GetLowestDelayPath() {
    auto gen = PathGenerator();
    return path_storage_->PathFromLinksOrDie(gen->NextPath(),
                                             std::get<2>(ie_key_));
  }

  // Will return the K lowest delay paths.
  std::vector<const GraphPath*> GetKLowestDelayPaths(size_t k) {
    auto gen = PathGenerator();
    std::vector<const GraphPath*> paths;
    for (size_t i = 0; i < k; ++i) {
      LinkSequence next_link_seq = gen->NextPath();
      const GraphPath* next_path = path_storage_->PathFromLinksOrDie(
          next_link_seq, std::get<2>(ie_key_));
      if (!next_path->empty()) {
        paths.emplace_back(next_path);
      }
    }

    return paths;
  }

  // Will return the lowest delay path (P) and any paths that are up to
  // hop_count(P) + k hops long.
  std::vector<const GraphPath*> GetPathsKHopsFromLowestDelay(size_t k);

  // Caches all paths between the source and the destination.
  const std::vector<net::LinkSequence>& CacheAll();

 private:
  IngressEgressPathCache(const PathCacheConfig& path_cache_config,
                         const IngressEgressKey& ie_key,
                         std::unique_ptr<Constraint> constraint,
                         const SimpleDirectedGraph* graph,
                         PathStorage* path_storage)
      : path_cache_config_(path_cache_config),
        ie_key_(ie_key),
        graph_(graph),
        path_storage_(path_storage),
        constraint_(std::move(constraint)) {}

  IngressEgressPathCache(const PathCacheConfig& path_cache_config,
                         const IngressEgressKey& ie_key,
                         const SimpleDirectedGraph* graph,
                         PathStorage* path_storage)
      : IngressEgressPathCache(path_cache_config, ie_key, DummyConstraint(),
                               graph, path_storage) {}

  std::unique_ptr<ShortestPathGenerator> PathGenerator();

  const PathCacheConfig path_cache_config_;
  const IngressEgressKey ie_key_;
  const SimpleDirectedGraph* graph_;
  PathStorage* path_storage_;

  // Constraint.
  std::unique_ptr<Constraint> constraint_;

  // Paths, ordered in increasing delay.
  std::vector<LinkSequence> paths_;

  friend class PathCache;
  DISALLOW_COPY_AND_ASSIGN(IngressEgressPathCache);
};

// An entity that will cache all possible paths between a source and
// a destination (if needed).
class PathCache {
 public:
  using ConstraintMap = std::map<IngressEgressKey, std::unique_ptr<Constraint>>;

  // Creates a new cache.
  PathCache(const PathCacheConfig& path_cache_config, PathStorage* path_storage,
            ConstraintMap* constraint_map = nullptr);

  // The graph.
  const SimpleDirectedGraph* graph() const { return &graph_; }

  // Path storage.
  PathStorage* path_storage() { return path_storage_; }

  // Returns the cache between two nodes.
  IngressEgressPathCache* IECache(const IngressEgressKey& key);

 private:
  const PathCacheConfig path_cache_config_;
  const SimpleDirectedGraph graph_;
  PathStorage* path_storage_;

  // Stores cached between a source and a destination.
  std::map<IngressEgressKey, std::unique_ptr<IngressEgressPathCache>>
      ie_caches_;

  DISALLOW_COPY_AND_ASSIGN(PathCache);
};

}  // namespace dfs
}  // namespace ncode

#endif /* NCODE_NET_PATH_CACHE */
