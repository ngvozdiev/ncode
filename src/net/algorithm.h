#ifndef NCODE_NET_ALGO_H
#define NCODE_NET_ALGO_H

#include <stdint.h>
#include <limits>

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

// Computes shortest paths between all pairs of nodes, can also be used to
// figure out if the graph is partitioned.
class AllPairShortestPath {
 public:
  AllPairShortestPath(const SimpleDirectedGraph* graph,
                      const GraphLinkSet& links_to_exclude = {},
                      const GraphNodeSet& nodes_to_exclude = {})
      : graph_(graph),
        links_to_exclude_(links_to_exclude),
        nodes_to_exclude_(nodes_to_exclude) {
    ComputePaths();
  }

  // Returns the shortest path between src and dst.
  LinkSequence GetPath(GraphNodeIndex src, GraphNodeIndex dst) const;

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

  // The graph.
  const SimpleDirectedGraph* graph_;

  // Links and nodes to exclude.
  const GraphLinkSet links_to_exclude_;
  const GraphNodeSet nodes_to_exclude_;

  // Distances to the destination.
  GraphNodeMap<GraphNodeMap<SPData>> data_;
};

// Single source shortest path.
class ShortestPath {
 public:
  ShortestPath(GraphNodeIndex src, const SimpleDirectedGraph* graph,
               const GraphLinkSet& links_to_exclude = {},
               const GraphNodeSet& nodes_to_exclude = {})
      : src_(src),
        graph_(graph),
        links_to_exclude_(links_to_exclude),
        nodes_to_exclude_(nodes_to_exclude) {
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

  // The graph.
  const SimpleDirectedGraph* graph_;

  // Links and nodes to exclude.
  const GraphLinkSet links_to_exclude_;
  const GraphNodeSet nodes_to_exclude_;

  // For each node, the link that leads to it in the SP tree.
  GraphNodeMap<GraphLinkIndex> previous_;

  // Delays from each node to the destination.
  GraphNodeMap<DistanceFromSource> min_delays_;
};

// Simple depth-limited DFS.
class DFS {
 public:
  using PathCallback = std::function<void(const LinkSequence&)>;

  DFS(const SimpleDirectedGraph* graph,
      const GraphLinkSet& links_to_exclude = {},
      const GraphNodeSet& nodes_to_exclude = {});

  // Calls a callback on all paths between a source and a destination.
  void Paths(GraphNodeIndex src, GraphNodeIndex dst, Delay max_distance,
             PathCallback path_callback) const;

 private:
  void PathsRecursive(Delay max_distance, GraphNodeIndex at, GraphNodeIndex dst,
                      PathCallback path_callback, GraphNodeSet* nodes_seen,
                      Links* current, Delay* total_distance) const;

  // The graph.
  const SimpleDirectedGraph* graph_;

  // Links and nodes to exclude.
  const GraphLinkSet links_to_exclude_;
  const GraphNodeSet nodes_to_exclude_;

  // The graph storage.
  const GraphStorage* storage_;

  // The shortest paths are used to prune the DFS like in A*.
  AllPairShortestPath all_pair_sp_;
};

}  // namespace net
}  // namespace ncode

#endif
