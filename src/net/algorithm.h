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

struct GraphSearchAlgorithmArgs {
  GraphLinkSet links_to_exclude;
  GraphNodeSet nodes_to_exclude;
};

class GraphSearchAlgorithm {
 protected:
  GraphSearchAlgorithm(const GraphSearchAlgorithmArgs& args,
                       const SimpleDirectedGraph* graph);

  // The graph.
  const SimpleDirectedGraph* graph_;

  // To exclude.
  GraphLinkSet links_to_exclude_;
  GraphNodeSet nodes_to_exclude_;
};

struct DeprefSearchAlgorithmArgs : GraphSearchAlgorithmArgs {
  GraphLinkSet links_to_depref;
  GraphNodeSet nodes_to_depref;
};

class DeprefSearchAlgorithm : public GraphSearchAlgorithm {
 protected:
  DeprefSearchAlgorithm(const DeprefSearchAlgorithmArgs& args,
                        const SimpleDirectedGraph* graph);

  // To depref.
  GraphLinkSet links_to_depref_;
  GraphNodeSet nodes_to_depref_;

  // Cost to assign to depreffed links.
  Delay depref_cost_;
};

// Computes shortest paths between all pairs of nodes, can also be used to
// figure out if the graph is partitioned.
class AllPairShortestPath : public DeprefSearchAlgorithm {
 public:
  AllPairShortestPath(const DeprefSearchAlgorithmArgs& args,
                      const SimpleDirectedGraph* graph)
      : DeprefSearchAlgorithm(args, graph) {
    ComputePaths();
  }

  // Returns the shortest path between src and dst. The second return value will
  // be set to false if the path fails to avoid all depref links/nodes,
  // otherwise unchanged.
  LinkSequence GetPath(GraphNodeIndex src, GraphNodeIndex dst,
                       bool* avoid_depref = nullptr) const;

  // Returns the length of the shortest path between src and dst.
  Delay GetDistance(GraphNodeIndex src, GraphNodeIndex dst,
                    bool* avoid_depref = nullptr) const;

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
class ShortestPath : public DeprefSearchAlgorithm {
 public:
  ShortestPath(const DeprefSearchAlgorithmArgs& args, GraphNodeIndex src,
               const SimpleDirectedGraph* graph)
      : DeprefSearchAlgorithm(args, graph), src_(src) {
    ComputePaths();
  }

  // Returns the shortest path to the destination.
  LinkSequence GetPath(GraphNodeIndex dst, bool* avoid_depref = nullptr) const;

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

// Simple depth-limited DFS.
class DFS : public GraphSearchAlgorithm {
 public:
  using PathCallback = std::function<void(const LinkSequence&)>;

  DFS(const GraphSearchAlgorithmArgs& args, const SimpleDirectedGraph* graph);

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
