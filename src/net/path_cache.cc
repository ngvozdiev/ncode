#include "path_cache.h"

#include <cstdbool>

#include "../common/logging.h"
#include "../common/perfect_hash.h"
#include "algorithm.h"

namespace ncode {
namespace net {

constexpr Delay IngressEgressPathCache::kMaxDistance;

LinkSequence IngressEgressPathCache::GetLowestDelayPath(
    const GraphLinkSet& to_avoid) {
  if (paths_.empty()) {
    return constraint_->ShortestCompliantPath(*graph_, to_avoid, src_, dst_);
  }

  std::vector<LinkSequence> k_lowest = GetKLowestDelayPaths(1, to_avoid);
  if (k_lowest.empty()) {
    return {};
  }

  return k_lowest.front();
}

static bool PathContainsAnyOfLinks(const GraphLinkSet& to_avoid,
                                   const LinkSequence& path) {
  for (GraphLinkIndex link : path.links()) {
    if (to_avoid.Contains(link)) {
      return true;
    }
  }

  return false;
}

std::vector<LinkSequence> IngressEgressPathCache::GetKLowestDelayPaths(
    size_t k, const GraphLinkSet& to_avoid) {
  std::vector<LinkSequence> return_vector;
  if (k == 0) {
    return return_vector;
  }

  return_vector.reserve(k);
  CacheAll();

  for (const auto& path : paths_) {
    if (!constraint_->PathComplies(path)) {
      continue;
    }

    if (PathContainsAnyOfLinks(to_avoid, path)) {
      continue;
    }

    return_vector.emplace_back(path);
    if (return_vector.size() == k) {
      break;
    }
  }

  return return_vector;
}

std::vector<LinkSequence> IngressEgressPathCache::GetPathsKHopsFromLowestDelay(
    size_t k, const GraphLinkSet& to_avoid) {
  std::vector<LinkSequence> return_vector;
  CacheAll();

  LinkSequence shortest_path = GetLowestDelayPath(to_avoid);
  size_t shortest_path_hop_count = shortest_path.size();
  size_t limit = shortest_path_hop_count + k;

  for (const auto& path : paths_) {
    if (path.size() > limit) {
      continue;
    }

    if (!constraint_->PathComplies(path)) {
      continue;
    }

    if (PathContainsAnyOfLinks(to_avoid, path)) {
      continue;
    }

    return_vector.emplace_back(path);
  }

  return return_vector;
}

const std::vector<LinkSequence>& IngressEgressPathCache::CacheAll() {
  if (!paths_.empty()) {
    return paths_;
  }

  DFS dfs(graph_);
  dfs.Paths(src_, dst_, kMaxDistance,
            [this](const LinkSequence& path) { paths_.emplace_back(path); });
  std::sort(paths_.begin(), paths_.end());

  return paths_;
}

PathCache::PathCache(const SimpleDirectedGraph* graph,
                     ConstraintMap* constraint_map)
    : graph_(graph) {
  if (constraint_map) {
    for (auto& src_dst_pair_and_constraint : *constraint_map) {
      GraphNodeIndex src = src_dst_pair_and_constraint.first.first;
      GraphNodeIndex dst = src_dst_pair_and_constraint.first.second;
      std::unique_ptr<Constraint> constraint =
          std::move(src_dst_pair_and_constraint.second);
      CHECK(constraint) << "No constraint";

      std::unique_ptr<IngressEgressPathCache>& ie_cache_ptr =
          ie_caches_[{src, dst}];
      ie_cache_ptr = make_unique<IngressEgressPathCache>(
          src, dst, std::move(constraint), graph_);
    }
  }
}

IngressEgressPathCache* PathCache::IECache(GraphNodeIndex src,
                                           GraphNodeIndex dst) {
  std::unique_ptr<IngressEgressPathCache>& ie_cache_ptr =
      ie_caches_[{src, dst}];
  if (!ie_cache_ptr) {
    ie_cache_ptr = make_unique<IngressEgressPathCache>(src, dst, graph_);
  }

  return ie_cache_ptr.get();
}

}  // namespace net
}  // namespace ncode
