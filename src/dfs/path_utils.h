#ifndef NCODE_DFS_PATH_UTILS_H
#define NCODE_DFS_PATH_UTILS_H

#include <stddef.h>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "dfs.pb.h"
#include "net.pb.h"
#include "../common/common.h"
#include "../net/net_common.h"

namespace ncode {
namespace dfs {
class ArrayGraph;
class Constraint;
} /* namespace dfs */
} /* namespace ncode */

namespace ncode {
namespace dfs {

// An entity that will cache all possible paths between a sources and
// destinations.
class PathCache {
 public:
  PathCache(const net::PBNet& graph, const PBDFSRequest& request_template,
            net::PathStorage* storage);

  // Will return the lowest delay path that meets a constraint.
  const net::GraphPath* GetLowestDelayPath(
      const Constraint& constraint, const std::string& src,
      const std::string& dst, uint64_t cookie,
      std::chrono::microseconds delay_limit = std::chrono::microseconds::max());

  // Will return the K lowest delay paths that meet a constraint.
  std::vector<const net::GraphPath*> GetKLowestDelayPaths(
      const Constraint& constraint, size_t k, const std::string& src,
      const std::string& dst, uint64_t cookie,
      std::chrono::microseconds delay_limit = std::chrono::microseconds::max());

  // Will return the lowest delay path (P) and any paths that are up to
  // hop_count(P) + k hops long.
  std::vector<const net::GraphPath*> GetPathsKHopsFromLowestDelay(
      const Constraint& constraint, size_t k, const std::string& src,
      const std::string& dst, uint64_t cookie,
      std::chrono::microseconds delay_limit = std::chrono::microseconds::max());

  // Will return the k lowest delay paths between the source and the destination
  // that share as few links as possible.
  std::vector<const net::GraphPath*> GetKDiversePaths(
      const Constraint& constraint, size_t k, const std::string& src,
      const std::string& dst, uint64_t cookie,
      std::chrono::microseconds delay_limit = std::chrono::microseconds::max());

  // Populates the cache with paths for all src-dst pairs.
  void CacheAllPaths();

  // The underlying path storage.
  net::PathStorage* storage() { return path_storage_; }

  // The graph.
  const net::PBNet& graph() { return graph_; }

  // The request template.
  const PBDFSRequest& request_template() { return request_template_; }

  // Caches all paths between the source and the destination that satisfy the
  // request.
  const std::vector<net::LinkSequence>& CacheAll(const std::string& src,
                                                 const std::string& dst);

 private:
  // Returns the number of paths and an estimate for the bytes occupied by them.
  PathCacheStats GetStats() const;

  // Populates dst_to_array_graph_.
  void InitArrayGraphs();

  // Stores paths between a source and a destination.
  std::map<std::string, std::map<std::string, std::vector<net::LinkSequence>>>
      cache_;

  // Array graphs, grouped by destination.
  std::map<std::string, std::unique_ptr<ArrayGraph>> dst_to_array_graph_;

  const net::PBNet graph_;
  const PBDFSRequest request_template_;
  net::PathStorage* path_storage_;

  // Needed for thread safety.
  std::mutex mu_;

  DISALLOW_COPY_AND_ASSIGN(PathCache);
};

}  // namespace dfs
}  // namespace ncode

#endif /* DFS_PATH_UTILS_H */
