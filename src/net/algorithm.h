#ifndef NCODE_NET_ALGO_H
#define NCODE_NET_ALGO_H

#include <stdint.h>
#include <limits>
#include <queue>

#include "net_common.h"

namespace ncode {
namespace net {

// A directed graph with no loops or multiple edges.
class SimpleDirectedGraph {
 public:
  SimpleDirectedGraph(const GraphStorage* parent);

  // Returns the adjacency list.
  const GraphNodeMap<std::vector<GraphLinkIndex>>& AdjacencyList() const {
    return adjacency_list_;
  }

  // The parent graph. Not owned by this object.
  const GraphStorage* graph_storage() const { return graph_storage_; }

 private:
  void ConstructAdjacencyList();

  const GraphStorage* graph_storage_;

  // For each node the edges that leave the node.
  GraphNodeMap<std::vector<GraphLinkIndex>> adjacency_list_;
};

class GraphSearchAlgorithmConfig {
 public:
  void AddToExcludeLinks(const GraphLinkSet* set) {
    link_sets_to_exclude_.emplace_back(set);
  }

  void AddToExcludeNodes(const GraphNodeSet* set) {
    node_sets_to_exclude_.emplace_back(set);
  }

  bool CanExcludeLink(const GraphLinkIndex link) const {
    for (const GraphLinkSet* set : link_sets_to_exclude_) {
      if (set->Contains(link)) {
        return true;
      }
    }

    return false;
  }

  bool CanExcludeNode(const GraphNodeIndex node) const {
    for (const GraphNodeSet* set : node_sets_to_exclude_) {
      if (set->Contains(node)) {
        return true;
      }
    }

    return false;
  }

 private:
  // Links/nodes that will be excluded from the graph.
  std::vector<const GraphLinkSet*> link_sets_to_exclude_;
  std::vector<const GraphNodeSet*> node_sets_to_exclude_;
};

class GraphSearchAlgorithm {
 protected:
  GraphSearchAlgorithm(const GraphSearchAlgorithmConfig& config,
                       const SimpleDirectedGraph* graph);

  // The graph.
  const SimpleDirectedGraph* graph_;

  // Configuration for the algorithm.
  GraphSearchAlgorithmConfig config_;
};

// Computes shortest paths between all pairs of nodes, can also be used to
// figure out if the graph is partitioned.
class AllPairShortestPath : public GraphSearchAlgorithm {
 public:
  AllPairShortestPath(const GraphSearchAlgorithmConfig& config,
                      const SimpleDirectedGraph* graph)
      : GraphSearchAlgorithm(config, graph) {
    ComputePaths();
  }

  // Returns the shortest path between src and dst. The second return value will
  // be set to false if the path fails to avoid all depref links/nodes,
  // otherwise unchanged.
  LinkSequence GetPath(GraphNodeIndex src, GraphNodeIndex dst) const;

  // Returns the length of the shortest path between src and dst.
  Delay GetDistance(GraphNodeIndex src, GraphNodeIndex dst) const;

 private:
  static constexpr Delay kMaxDistance = Delay::max();

  struct SPData {
    SPData() : distance(kMaxDistance) {}

    // Distance between the 2 endpoints.
    Delay distance;

    // Successor in the SP.
    GraphLinkIndex next_link;
    GraphNodeIndex next_node;
  };

  // Populates data_.
  void ComputePaths();

  // Distances to the destination.
  GraphNodeMap<GraphNodeMap<SPData>> data_;
};

// Single source shortest path.
class ShortestPath : public GraphSearchAlgorithm {
 public:
  ShortestPath(const GraphSearchAlgorithmConfig& config, GraphNodeIndex src,
               const SimpleDirectedGraph* graph)
      : GraphSearchAlgorithm(config, graph), src_(src) {
    ComputePaths();
  }

  // Returns the shortest path to the destination.
  LinkSequence GetPath(GraphNodeIndex dst) const;

 private:
  struct DistanceFromSource {
    DistanceFromSource() : distance(Delay::max()) {}
    Delay distance;
  };

  void ComputePaths();

  // The source.
  GraphNodeIndex src_;

  // For each node, the link that leads to it in the SP tree.
  GraphNodeMap<GraphLinkIndex> previous_;

  // Delays from each node to the destination.
  GraphNodeMap<DistanceFromSource> min_delays_;
};

// Returns the single shortest path that goes through a series of links in the
// given order or returns an empty path if no such path exists.
LinkSequence WaypointShortestPath(const GraphSearchAlgorithmConfig& config,
                                  const Links& waypoints, GraphNodeIndex src,
                                  GraphNodeIndex dst,
                                  const SimpleDirectedGraph* graph);

// K shortest paths that optionally go through a set of waypoints.
class KShortestPaths : public GraphSearchAlgorithm {
 public:
  KShortestPaths(const GraphSearchAlgorithmConfig& config,
                 const Links& waypoints, GraphNodeIndex src, GraphNodeIndex dst,
                 const SimpleDirectedGraph* graph);

  // Returns the next path.
  LinkSequence NextPath();

 private:
  using PathAndStartIndex = std::pair<LinkSequence, size_t>;

  // Returns true if prefix_path[0:index] == path[0:index]
  static bool HasPrefix(const Links& path, const Links& prefix);

  // Returns a set of links that contains: for any path in k_paths_ that starts
  // with the same links as root_path pick the next link -- the one after.
  void GetLinkExclusionSet(const Links& root_path, GraphLinkSet* out);

  // Waypoints.
  const std::vector<GraphLinkIndex> waypoints_;

  // The source.
  GraphNodeIndex src_;

  // The destination.
  GraphNodeIndex dst_;

  // Stores the K shortest paths in order.
  std::vector<PathAndStartIndex> k_paths_;

  // Stores candidates for K shortest paths.
  std::priority_queue<PathAndStartIndex, std::vector<PathAndStartIndex>,
                      std::greater<PathAndStartIndex>> candidates_;
};

// Simple depth-limited DFS.
class DFS : public GraphSearchAlgorithm {
 public:
  using PathCallback = std::function<void(const LinkSequence&)>;

  DFS(const GraphSearchAlgorithmConfig& config,
      const SimpleDirectedGraph* graph);

  // Calls a callback on all paths between a source and a destination.
  void Paths(GraphNodeIndex src, GraphNodeIndex dst, Delay max_distance,
             size_t max_hops, PathCallback path_callback) const;

 private:
  void PathsRecursive(Delay max_distance, size_t max_hops, GraphNodeIndex at,
                      GraphNodeIndex dst, PathCallback path_callback,
                      GraphNodeSet* nodes_seen, Links* current,
                      Delay* total_distance) const;

  // The graph storage.
  const GraphStorage* storage_;

  // The shortest paths are used to prune the DFS like in A*.
  AllPairShortestPath all_pair_sp_;
};

}  // namespace net
}  // namespace ncode

#endif
