#ifndef NCODE_AGRAPH_H
#define NCODE_AGRAPH_H

#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <valarray>
#include <vector>

#include "../common/common.h"
#include "../net/net_common.h"

namespace ncode {
namespace net {
class PathStorage;
} /* namespace net */
} /* namespace ncode */

namespace ncode {
namespace dfs {

using std::string;
using std::vector;
using std::map;

typedef int ArrayGraphOffset;
constexpr int kArrayGraphInfiniteDistance = std::numeric_limits<int>::max();

// An array graph is a fast representation of an immutable graph. It allows for
// quick retrieval of adjacency information and hop counts.
class ArrayGraph {
 public:
  // Creates a new ArrayGraph from a graph protobuf.
  static std::unique_ptr<ArrayGraph> NewArrayGraph(const net::PBNet& graph,
                                                   const std::string& dst,
                                                   net::PathStorage* storage) {
    auto agraph = NewArrayGraphPrivate(graph, dst, false, storage);
    agraph->PopulateDistanceToDestination(graph, dst);
    return agraph;
  }

  // Is a graph vertex marked. Complexity is O(1).
  bool IsVertexMarked(ArrayGraphOffset offset) const {
    return graph_[offset + kMarkOffset] == 1;
  }

  // Sets the mark on a graph vertex. Complexity is O(1).
  void MarkVertex(ArrayGraphOffset offset) { graph_[offset + kMarkOffset] = 1; }

  // Removes the mark from a graph vertex. Complexity is O(1).
  void UnmarkVertex(ArrayGraphOffset offset) {
    graph_[offset + kMarkOffset] = 0;
  }

  // Clears the marks from all vertices in the graph. Complexity is O(n).
  void UnmarkAllVertices() {
    ArrayGraphOffset offset = 0;
    for (int i = 0; i < num_cells_; i++) {
      UnmarkVertex(offset);

      // Each neighbor occupies 3 int values - the index of the edge, the offset
      // of the neighbor and the weight of the edge
      offset += kCellSize + kNeighborSize * GetNeighborCount(offset);
    }
  }

  // Returns the number of neighbors of a vertex at a given offset.
  // Complexity is O(1).
  int GetNeighborCount(ArrayGraphOffset offset) const {
    return graph_[offset + kNeighborCountOffset];
  }

  // Gets the distance from a vertex at a given offset to the destination.
  int GetDistanceToDest(ArrayGraphOffset offset) const {
    return graph_[offset + kDistanceOffset];
  }

  // Returns the index of the edge to the n-th neighbor of a vertex at a given
  // offset. The first neighbor is n = 0.
  net::GraphLinkIndex GetIndexOfEdge(ArrayGraphOffset offset, int n) const {
    return net::GraphLinkIndex(graph_[offset + kCellSize + kNeighborSize * n]);
  }

  // Returns the offset of the n-th neighbor of a vertex at a given offset.
  // Indexing starts from 0 the first neighbor is n = 0.
  ArrayGraphOffset GetOffsetOfNeighbor(ArrayGraphOffset offset, int n) const {
    return graph_[offset + kCellSize + kNeighborSize * n + 1];
  }

  // Returns the weight of the link from a vertex at a given offset to its
  // n-th neighbor.
  int GetDistanceToNeighbor(int offset, int n) const {
    return graph_[offset + kCellSize + kNeighborSize * n + 2];
  }

  const map<string, ArrayGraphOffset>& vertex_id_to_offset() const {
    return vertex_id_to_offset_;
  }

  const map<ArrayGraphOffset, string>& offset_to_vertex_id() const {
    return offset_to_vertex_id_;
  }

  ArrayGraphOffset dst_offset() const { return dst_offset_; }

  net::PathStorage* storage() const { return storage_; }

 private:
  // A map from the id of a vertex to a list of <neighbor id,
  // edge to the neighbor> pairs.
  typedef map<string, vector<std::pair<string, net::GraphLinkIndex>>>
      NeighborMap;

  ArrayGraph(const std::string& dst, const std::unordered_set<string>& vertices,
             const NeighborMap& neighbor_map, net::PathStorage* storage);

  // A private version of NewArrayGraph that takes as an argument whether edges
  // should be flipped upon construction.
  static std::unique_ptr<ArrayGraph> NewArrayGraphPrivate(
      const net::PBNet& graph, const std::string& dst, bool reverse_edges,
      net::PathStorage* storage);

  // Offset from the start of the cell to the node's mark.
  static const int kMarkOffset = 0;

  // Offset from the start of the cell to the distance to
  // destination (in terms of metric).
  static const int kDistanceOffset = 1;

  // Offset from the start of the cell to the number of neighbors the node has
  static const int kNeighborCountOffset = 2;

  // Total size of the cell
  static const int kCellSize = 3;

  // How many elements each neighbor occupies
  static const int kNeighborSize = 3;

  net::PathStorage* storage_;

  // The graph is represented as a contiguous array of integers. Each vertex has
  // a separate fixed-size cell (chunk of the array) followed by for each
  // neighbor (vertex that it can directly reach) a tuple of the index of the
  // edge (a linear indexing starting at 0) the offset of the neighbor and the
  // distance to it. The layout is as follows: [mark, distance to destination,
  // neighbor count, index of the edge to n1, offset of n1 into graph_,
  // weigth of edge to n1, index of edge to n2, offset of n2 into graph_,
  // weight of link to n2, index of edge to n3, ... next_vertex_id,
  // next_vertex_mark, ....].
  std::valarray<int> graph_;

  // Mapping from vertex ids to offsets. The ids come from the original
  // protobuf.
  map<string, ArrayGraphOffset> vertex_id_to_offset_;

  // Mapping from offsets back to vertex ids.
  map<ArrayGraphOffset, string> offset_to_vertex_id_;

  // Number of cells in the graph. This is the same as the number of vertices.
  int num_cells_;

  // Offset into graph_ of the destination vertex.
  ArrayGraphOffset dst_offset_;

  // Sets the number of neighbors of a vertex.
  void SetNeighborCount(ArrayGraphOffset offset, int count) {
    graph_[offset + kNeighborCountOffset] = count;
  }

  // Sets the distance to the destination.
  void SetDistanceToDest(ArrayGraphOffset offset, int count) {
    graph_[offset + kDistanceOffset] = count;
  }

  // Sets the offset and the distance to the n-th neighbor of a vertex.
  void SetNeighbor(ArrayGraphOffset offset, int n,
                   net::GraphLinkIndex edge_index,
                   ArrayGraphOffset neighbor_offset, int distance) {
    graph_[offset + kCellSize + kNeighborSize * n] = edge_index;
    graph_[offset + kCellSize + kNeighborSize * n + 1] = neighbor_offset;
    graph_[offset + kCellSize + kNeighborSize * n + 2] = distance;
  }

  // Runs shortest path and populates at all vertices the distance to the vertex
  // at the specified offset.
  void PopulateDistanceToDestination(const net::PBNet& graph,
                                     const std::string& dst);

  // Runs SP and populates at each vertex the distance from a source vertex
  // to that vertex.
  void PopulateDistanceFromSource(ArrayGraphOffset vertex_src);

  // Orders the neighbors of the vertex at an offset by the distance to the
  // destination.
  void OrderNeighborsByDistanceToDest(ArrayGraphOffset offset);

  DISALLOW_COPY_AND_ASSIGN(ArrayGraph);
};

// Returns a copy of a Graph that contains a subset of the edges in the original
// that form a tree rooted at a given node. There are no guarantees about the
// optimality of the resulting tree. If the root is invalid an exception is
// thrown.
net::PBNet ToTree(const net::PBNet& graph, const std::string& root,
                  net::PathStorage* storage);

}  // namespace dfs
}  // namespace ncode

#endif /* NCODE_AGRAPH_H */
