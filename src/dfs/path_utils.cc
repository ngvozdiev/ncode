#include "path_utils.h"

#include <google/protobuf/repeated_field.h>
#include <algorithm>
#include <iostream>
#include <set>
#include <stdexcept>

#include "../common/map_util.h"
#include "../common/substitute.h"
#include "array_graph.h"
#include "constraint.h"
#include "dfs.h"

namespace ncode {
namespace dfs {

static constexpr std::chrono::microseconds kDiversePathsDelayPenalty(1000000);

PathCache::PathCache(const net::PBNet& graph,
                     const PBDFSRequest& request_template,
                     net::PathStorage* storage)
    : graph_(graph),
      request_template_(request_template),
      path_storage_(storage) {
  InitArrayGraphs();
}

const net::GraphPath* PathCache::GetLowestDelayPath(
    const Constraint& constraint, const std::string& src,
    const std::string& dst, uint64_t cookie,
    std::chrono::microseconds delay_limit) {
  const std::vector<net::LinkSequence>& paths = CacheAll(src, dst);
  for (const auto& links : paths) {
    if (links.delay() > delay_limit) {
      break;
    }

    if (constraint.PathComplies(links)) {
      return path_storage_->PathFromLinks(links, cookie);
    }
  }

  return path_storage_->EmptyPath();
}

std::vector<const net::GraphPath*> PathCache::GetKLowestDelayPaths(
    const Constraint& constraint, size_t k, const std::string& src,
    const std::string& dst, uint64_t cookie,
    std::chrono::microseconds delay_limit) {
  std::vector<const net::GraphPath*> return_vector;
  if (k == 0) {
    return return_vector;
  }

  return_vector.reserve(k);
  const std::vector<net::LinkSequence>& paths = CacheAll(src, dst);
  for (const auto& links : paths) {
    if (links.delay() > delay_limit) {
      break;
    }

    if (constraint.PathComplies(links)) {
      return_vector.emplace_back(path_storage_->PathFromLinks(links, cookie));
      if (return_vector.size() == k) {
        break;
      }
    }
  }

  return return_vector;
}

// Helper function for GetKDiversePaths. Will determine what the delay penalty
// should be for a LinkSquence by applying a penalty of
// kDiversePathsDelayPenalty for each link that is contained in the
// links_to_avoid argument.
static std::chrono::microseconds GetDiversePathsDelayPenalty(
    std::set<const net::GraphLink*>& links_to_avoid,
    const net::LinkSequence& link_sequence) {
  std::chrono::microseconds total_delay_penalty(0);
  for (const net::GraphLink* link : link_sequence.links()) {
    if (ContainsKey(links_to_avoid, link)) {
      total_delay_penalty += kDiversePathsDelayPenalty;
    }
  }

  return total_delay_penalty;
}

struct LinkSequenceAndPenalty {
  LinkSequenceAndPenalty(const net::LinkSequence* link_sequence)
      : link_sequence(link_sequence), delay_plus_penalty(0) {}

  const net::LinkSequence* link_sequence;
  std::chrono::microseconds delay_plus_penalty;
};

std::vector<const net::GraphPath*> PathCache::GetKDiversePaths(
    const Constraint& constraint, size_t k, const std::string& src,
    const std::string& dst, uint64_t cookie,
    std::chrono::microseconds delay_limit) {
  std::set<const net::GraphPath*> return_paths;

  const std::vector<net::LinkSequence>& paths = CacheAll(src, dst);
  std::vector<LinkSequenceAndPenalty> scratch;
  scratch.reserve(paths.size());

  for (const auto& links : paths) {
    if (links.delay() > delay_limit) {
      continue;
    }

    if (constraint.PathComplies(links)) {
      scratch.emplace_back(&links);
    }
  }

  std::set<const net::GraphLink*> links_to_avoid;
  for (size_t i = 0; i < k; ++i) {
    for (LinkSequenceAndPenalty& link_sequence_and_penalty : scratch) {
      const net::LinkSequence& link_sequence =
          *link_sequence_and_penalty.link_sequence;

      std::chrono::microseconds penalty =
          GetDiversePathsDelayPenalty(links_to_avoid, link_sequence);
      link_sequence_and_penalty.delay_plus_penalty =
          link_sequence.delay() + penalty;
    }

    std::sort(scratch.begin(), scratch.end(),
              [&links_to_avoid](const LinkSequenceAndPenalty& lhs,
                                const LinkSequenceAndPenalty& rhs) {
                return lhs.delay_plus_penalty < rhs.delay_plus_penalty;
              });

    const net::LinkSequence& best_link_sequence =
        *scratch.begin()->link_sequence;
    for (const net::GraphLink* link : best_link_sequence.links()) {
      links_to_avoid.insert(link);
    }

    const net::GraphPath* new_path =
        path_storage_->PathFromLinks(best_link_sequence, cookie);
    return_paths.insert(new_path);
  }

  // The return paths set can contain less than k paths. In this case we will
  // "top it up" from the k shortest paths.
  if (return_paths.size() != k) {
    for (const net::LinkSequence& link_sequence : paths) {
      const net::GraphPath* new_path =
          path_storage_->PathFromLinks(link_sequence, cookie);
      return_paths.insert(new_path);
      if (return_paths.size() == k) {
        break;
      }
    }
  }

  std::vector<const net::GraphPath*> return_vector;
  return_vector.insert(return_vector.end(), return_paths.begin(),
                       return_paths.end());
  std::sort(
      return_vector.begin(), return_vector.end(),
      [&links_to_avoid](const net::GraphPath* lhs, const net::GraphPath* rhs) {
        return lhs->delay() < rhs->delay();
      });

  return return_vector;
}

std::vector<const net::GraphPath*> PathCache::GetPathsKHopsFromLowestDelay(
    const Constraint& constraint, size_t k, const std::string& src,
    const std::string& dst, uint64_t cookie,
    std::chrono::microseconds delay_limit) {
  std::vector<const net::GraphPath*> return_vector;
  const std::vector<net::LinkSequence>& paths = CacheAll(src, dst);
  const net::GraphPath* shortest_path =
      GetLowestDelayPath(constraint, src, dst, cookie);
  return_vector.push_back(shortest_path);
  size_t shortest_path_hop_count = shortest_path->size();
  size_t limit = shortest_path_hop_count + k;

  for (const auto& links : paths) {
    if (links.delay() > delay_limit) {
      break;
    }

    if (links.size() > limit) {
      continue;
    }

    if (constraint.PathComplies(links)) {
      const net::GraphPath* path = path_storage_->PathFromLinks(links, cookie);
      if (path->tag() != shortest_path->tag()) {
        return_vector.emplace_back(path);
      }
    }
  }

  return return_vector;
}

void PathCache::CacheAllPaths() {
  std::set<std::string> all_devices;
  for (const auto& link : graph_.links()) {
    all_devices.insert(link.src());
    all_devices.insert(link.dst());
  }

  for (const auto& src : all_devices) {
    for (const auto& dst : all_devices) {
      if (src != dst) {
        CacheAll(src, dst);
      }
    }
  }

  PathCacheStats stats = GetStats();
  LOG(INFO) << Substitute("Cached $0 paths, $1 bytes", stats.num_paths(),
                          stats.bytes_used());
}

void PathCache::InitArrayGraphs() {
  std::set<std::string> devices;
  for (const auto& link : graph_.links()) {
    devices.insert(link.dst());
    devices.insert(link.src());
  }

  for (const auto& dst : devices) {
    auto array_graph = ArrayGraph::NewArrayGraph(graph_, dst, path_storage_);
    dst_to_array_graph_[dst] = std::move(array_graph);
  }
}

PathCacheStats PathCache::GetStats() const {
  using namespace std::chrono;

  PathCacheStats stats;
  std::vector<uint32_t> all_path_hops;
  std::vector<double> all_path_delays_sec;

  for (const auto& src_and_rest : cache_) {
    for (const auto& dst_and_paths : src_and_rest.second) {
      const std::vector<net::LinkSequence>& paths = dst_and_paths.second;
      size_t bytes_est = (paths.capacity() *
                          sizeof(std::vector<net::LinkSequence>::value_type));
      std::vector<uint32_t> path_hops;
      std::vector<double> path_delays_sec;
      path_hops.reserve(paths.size());
      path_delays_sec.reserve(paths.size());
      for (const auto& path : paths) {
        bytes_est += path.InMemBytesEstimate();
        path_hops.emplace_back(path.size());

        double delay_sec = duration<double>(path.delay()).count();
        path_delays_sec.emplace_back(delay_sec);
      }

      stats.set_bytes_used(stats.bytes_used() + bytes_est);
      stats.set_num_paths(stats.num_paths() + paths.size());

      SrcDstPairPathCacheStats* pair_stats = stats.add_src_dst_pair_stats();
      pair_stats->set_src(src_and_rest.first);
      pair_stats->set_dst(dst_and_paths.first);
      pair_stats->set_num_paths(paths.size());
      Percentiles(&path_hops,
                  pair_stats->mutable_path_length_hops_distribution());
      Percentiles(&path_delays_sec,
                  pair_stats->mutable_path_length_sec_distribution());

      all_path_hops.insert(all_path_hops.end(), path_hops.begin(),
                           path_hops.end());
      all_path_delays_sec.insert(all_path_delays_sec.end(),
                                 path_delays_sec.begin(),
                                 path_delays_sec.end());
    }
  }

  Percentiles(&all_path_hops, stats.mutable_path_length_hops_distribution());
  Percentiles(&all_path_delays_sec,
              stats.mutable_path_length_sec_distribution());
  return stats;
}

const std::vector<net::LinkSequence>& PathCache::CacheAll(
    const std::string& src, const std::string& dst) {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<net::LinkSequence>& paths = cache_[src][dst];
  if (!paths.empty()) {
    return paths;
  }

  const auto& it = dst_to_array_graph_.find(dst);
  CHECK(it != dst_to_array_graph_.end()) << "Bad destination " + dst;

  const ArrayGraph* array_graph = it->second.get();
  PBDFSRequest request = request_template_;
  request.set_src(src);

  dfs::PathFoundCallback f = [&paths](const net::LinkSequence& links) {
    paths.emplace_back(links);
    return true;
  };

  auto dfs = make_unique<DFS>(request, *array_graph, f);
  dfs->Search();
  std::sort(paths.begin(), paths.end(),
            [](const net::LinkSequence& lhs, const net::LinkSequence& rhs) {
              return lhs.delay() < rhs.delay();
            });
  return paths;
}

}  // namespace dfs
}  // namespace ncode
