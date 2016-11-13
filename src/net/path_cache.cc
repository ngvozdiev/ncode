#include "path_cache.h"

#include <cstdbool>

#include "../common/logging.h"
#include "../common/perfect_hash.h"
#include "algorithm.h"

namespace ncode {
namespace net {

const GraphPath* IngressEgressPathCache::GetLowestDelayPath(
    const GraphLinkSet& to_avoid, bool* avoids) {
  net::LinkSequence shortest_path = constraint_->ShortestCompliantPath(
      *graph_, to_avoid, std::get<0>(ie_key_), std::get<1>(ie_key_), avoids);
  return path_storage_->PathFromLinksOrDie(shortest_path, std::get<2>(ie_key_));
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

std::vector<const GraphPath*> IngressEgressPathCache::GetKLowestDelayPaths(
    size_t k, const GraphLinkSet& to_avoid, bool* avoids) {
  CHECK(avoids == nullptr) << "Not supported yet";
  std::vector<const GraphPath*> return_vector;
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

    return_vector.emplace_back(
        path_storage_->PathFromLinksOrDie(path, std::get<2>(ie_key_)));
    if (return_vector.size() == k) {
      break;
    }
  }

  return return_vector;
}

std::vector<const GraphPath*>
IngressEgressPathCache::GetPathsKHopsFromLowestDelay(
    size_t k, const GraphLinkSet& to_avoid, bool* avoids) {
  CHECK(avoids == nullptr) << "Not supported yet";
  std::vector<const GraphPath*> return_vector;
  CacheAll();

  const GraphPath* shortest_path = GetLowestDelayPath(to_avoid, avoids);
  size_t shortest_path_hop_count = shortest_path->size();
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

    return_vector.emplace_back(
        path_storage_->PathFromLinksOrDie(path, std::get<2>(ie_key_)));
  }

  return return_vector;
}

const std::vector<net::LinkSequence>& IngressEgressPathCache::CacheAll() {
  if (!paths_.empty()) {
    return paths_;
  }

  DFS dfs({}, graph_);
  dfs.Paths(std::get<0>(ie_key_), std::get<1>(ie_key_),
            path_cache_config_.max_delay, path_cache_config_.max_hops,
            [this](const LinkSequence& path) { paths_.emplace_back(path); });
  std::sort(paths_.begin(), paths_.end());

  return paths_;
}

PathCache::PathCache(const PathCacheConfig& path_cache_config,
                     PathStorage* path_storage, ConstraintMap* constraint_map)
    : path_cache_config_(path_cache_config),
      graph_(path_storage),
      path_storage_(path_storage) {
  if (constraint_map) {
    for (auto& key_and_constraint : *constraint_map) {
      const IngressEgressKey& ie_key = key_and_constraint.first;
      std::unique_ptr<Constraint> constraint =
          std::move(key_and_constraint.second);
      CHECK(constraint) << "No constraint";

      std::unique_ptr<IngressEgressPathCache>& ie_cache_ptr =
          ie_caches_[ie_key];
      ie_cache_ptr =
          std::unique_ptr<IngressEgressPathCache>(new IngressEgressPathCache(
              path_cache_config_, ie_key, std::move(constraint), &graph_,
              path_storage_));
    }
  }
}

IngressEgressPathCache* PathCache::IECache(const IngressEgressKey& ie_key) {
  std::unique_ptr<IngressEgressPathCache>& ie_cache_ptr = ie_caches_[ie_key];
  if (!ie_cache_ptr) {
    ie_cache_ptr =
        std::unique_ptr<IngressEgressPathCache>(new IngressEgressPathCache(
            path_cache_config_, ie_key, &graph_, path_storage_));
  }

  return ie_cache_ptr.get();
}

}  // namespace net
}  // namespace ncode
