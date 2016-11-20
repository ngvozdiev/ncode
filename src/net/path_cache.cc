#include "path_cache.h"

#include <cstdbool>

#include "../common/logging.h"
#include "../common/perfect_hash.h"
#include "algorithm.h"

namespace ncode {
namespace net {

const LinkSequence* NodePairPathCache::KthShortestPathOrNull(
    size_t k, const GraphLinkSet* to_exclude) {
  // First we will try to figure out if the path is already in the cache.
  size_t i = 0;
  for (const auto& path_in_cache : paths_) {
    if (to_exclude && path_in_cache->ContainsAny(*to_exclude)) {
      continue;
    }

    if (i++ == k) {
      return path_in_cache.get();
    }
  }

  // Will have to extend the cache to include all paths up to the kth one that
  // complies with 'to_exclude'.
  while (true) {
    auto next_path = make_unique<LinkSequence>(path_generator_->NextPath());
    if (next_path->empty()) {
      // No more paths.
      return nullptr;
    }

    bool not_compliant = to_exclude && next_path->ContainsAny(*to_exclude);
    paths_.emplace_back(std::move(next_path));
    if (not_compliant) {
      continue;
    }

    if (i++ == k) {
      return paths_.back().get();
    }
  }
}

std::vector<const LinkSequence*> NodePairPathCache::Paths(
    size_t start_k, size_t* next_index, const GraphLinkSet* exclude) {
  std::vector<const LinkSequence*> out;

  size_t i = start_k;
  while (true) {
    const LinkSequence* next_path = GetPathAtIndexOrNull(i++);
    if (next_path == nullptr) {
      return {};
    }

    out.emplace_back(next_path);
    if (exclude && next_path->ContainsAny(*exclude)) {
      continue;
    }

    *next_index = i;
    break;
  }

  return out;
}

const LinkSequence* NodePairPathCache::GetPathAtIndexOrNull(size_t i) {
  while (i >= paths_.size()) {
    auto next_path = make_unique<LinkSequence>(path_generator_->NextPath());
    if (next_path->empty()) {
      // Ran out of paths before reaching i.
      return nullptr;
    }

    paths_.emplace_back(std::move(next_path));
  }

  return paths_[i].get();
}

std::vector<LinkSequence> NodePairPathCache::PathsKHopsFromShortest(
    size_t k) const {
  size_t shortest_path_hop_count = PathGenerator()->NextPath().size();
  size_t limit = shortest_path_hop_count + k;
  return AllPaths(limit);
}

std::vector<LinkSequence> NodePairPathCache::AllPaths(size_t max_hops) const {
  std::vector<LinkSequence> out;

  DFS dfs({}, graph_);
  dfs.Paths(key_.first, key_.second, Delay::max(), max_hops,
            [this, &out](const LinkSequence& path) {
              if (constraint_->PathComplies(path)) {
                out.emplace_back(path);
              }
            });
  std::sort(out.begin(), out.end());
  return out;
}

std::unique_ptr<ShortestPathGenerator> NodePairPathCache::PathGenerator()
    const {
  return constraint_->PathGenerator(*graph_, key_.first, key_.second, nullptr);
}

NodePairPathCache::NodePairPathCache(const NodePair& key,
                                     std::unique_ptr<Constraint> constraint,
                                     const SimpleDirectedGraph* graph,
                                     GraphStorage* path_storage)
    : key_(key),
      graph_(graph),
      graph_storage_(path_storage),
      constraint_(std::move(constraint)) {
  path_generator_ = PathGenerator();
}

PathCache::PathCache(GraphStorage* graph_storage, ConstraintMap* constraint_map)
    : graph_(graph_storage), graph_storage_(graph_storage) {
  if (constraint_map) {
    for (auto& key_and_constraint : *constraint_map) {
      const NodePair& ie_key = key_and_constraint.first;
      std::unique_ptr<Constraint> constraint =
          std::move(key_and_constraint.second);
      CHECK(constraint) << "No constraint";

      std::unique_ptr<NodePairPathCache>& ie_cache_ptr = ie_caches_[ie_key];
      ie_cache_ptr = std::unique_ptr<NodePairPathCache>(new NodePairPathCache(
          ie_key, std::move(constraint), &graph_, graph_storage_));
    }
  }
}

NodePairPathCache* PathCache::NodePairCache(const NodePair& ie_key) {
  std::unique_ptr<NodePairPathCache>& ie_cache_ptr = ie_caches_[ie_key];
  if (!ie_cache_ptr) {
    ie_cache_ptr = std::unique_ptr<NodePairPathCache>(
        new NodePairPathCache(ie_key, &graph_, graph_storage_));
  }

  return ie_cache_ptr.get();
}

}  // namespace net
}  // namespace ncode
