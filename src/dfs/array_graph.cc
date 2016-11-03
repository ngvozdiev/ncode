#include "array_graph.h"

#include <stddef.h>
#include <deque>
#include <set>

#include "../net/net_common.h"

namespace ncode {
namespace dfs {

using std::string;
using std::vector;

ArrayGraph::ArrayGraph(const string& dst,
                       const std::unordered_set<string>& vertices,
                       const NeighborMap& neighbor_map,
                       net::PathStorage* storage)
    : storage_(storage) {
  // Need a copy for convenience
  // TODO: Get rid of the copy if performance is an issue
  NeighborMap neighbor_map_copy = neighbor_map;

  int offset_total = 0;
  for (const string& vertex_id : vertices) {
    int num_neighbors = neighbor_map_copy[vertex_id].size();
    vertex_id_to_offset_[vertex_id] = offset_total;
    offset_to_vertex_id_[offset_total] = vertex_id;

    offset_total += kCellSize + kNeighborSize * num_neighbors;
  }

  num_cells_ = vertices.size();
  graph_.resize(offset_total);

  for (const auto& vertex_id_and_offset : vertex_id_to_offset_) {
    const string& vertex_id = vertex_id_and_offset.first;
    ArrayGraphOffset offset = vertex_id_and_offset.second;
    const auto& neighbors = neighbor_map_copy[vertex_id];

    UnmarkVertex(offset);
    SetNeighborCount(offset, neighbors.size());
    for (size_t i = 0; i < neighbors.size(); ++i) {
      const string& neighbor = neighbors.at(i).first;
      net::GraphLinkIndex edge_index = neighbors.at(i).second;
      const net::GraphLink* edge = storage->GetLink(edge_index);

      uint64_t delay_raw = edge->delay().count();
      CHECK(delay_raw <= std::numeric_limits<int>::max())
          << "Link delay too large";
      int weight = delay_raw;
      SetNeighbor(offset, i, edge_index, vertex_id_to_offset_[neighbor],
                  weight);
    }
  }

  dst_offset_ = vertex_id_to_offset_[dst];
}

std::unique_ptr<ArrayGraph> ArrayGraph::NewArrayGraphPrivate(
    const net::PBNet& graph, const string& dst, bool reverse_edges,
    net::PathStorage* storage) {
  std::unordered_set<string> vertices;

  // A map from the id of a vertex to a list of <neighbor id,
  // edge to the neighbor> pairs.
  NeighborMap neighbor_map;

  for (const auto& link_pb : graph.links()) {
    net::GraphLinkIndex link = storage->LinkFromProtobuf(link_pb);

    vertices.insert(link_pb.src());
    vertices.insert(link_pb.dst());

    if (reverse_edges) {
      neighbor_map[link_pb.dst()].push_back({link_pb.src(), link});
    } else {
      neighbor_map[link_pb.src()].push_back({link_pb.dst(), link});
    }
  }

  CHECK(vertices.count(dst)) << "Destination not in graph";
  std::unique_ptr<ArrayGraph> agraph(
      new ArrayGraph(dst, vertices, neighbor_map, storage));
  return agraph;
}

void ArrayGraph::PopulateDistanceToDestination(const net::PBNet& graph,
                                               const std::string& dst) {
  // We need to find the lengths of the SP from all vertices to the destination.
  // To do this we will reverse all edges and compute the SP from the
  // destination to all vertices.
  auto agraph_reverse = NewArrayGraphPrivate(graph, dst, true, storage_);

  ArrayGraphOffset reverse_offset = agraph_reverse->vertex_id_to_offset_[dst];
  agraph_reverse->PopulateDistanceFromSource(reverse_offset);

  // The offsets may be different in the reverse graph compared to the regular
  // one if there are unidirectional links.
  for (const auto& vertex_id_and_offset :
       agraph_reverse->vertex_id_to_offset_) {
    const string& vertex_id = vertex_id_and_offset.first;
    ArrayGraphOffset reverse_offset = vertex_id_and_offset.second;
    ArrayGraphOffset index = vertex_id_to_offset_[vertex_id];

    int distance = agraph_reverse->GetDistanceToDest(reverse_offset);
    SetDistanceToDest(index, distance);
  }

  for (const auto& vertex_id_and_offset : vertex_id_to_offset_) {
    OrderNeighborsByDistanceToDest(vertex_id_and_offset.second);
  }
}

void ArrayGraph::PopulateDistanceFromSource(ArrayGraphOffset src_offset) {
  for (const auto& vertex_id_and_offset : vertex_id_to_offset_) {
    ArrayGraphOffset index = vertex_id_and_offset.second;

    SetDistanceToDest(index, kArrayGraphInfiniteDistance);
  }

  SetDistanceToDest(src_offset, 0);

  int curr_offset = src_offset;

  while (!IsVertexMarked(curr_offset)) {
    MarkVertex(curr_offset);

    for (int i = 0; i < GetNeighborCount(curr_offset); i++) {
      int neighbor = GetOffsetOfNeighbor(curr_offset, i);
      int dist = GetDistanceToDest(curr_offset) +
                 GetDistanceToNeighbor(curr_offset, i);

      if (GetDistanceToDest(neighbor) > dist) {
        SetDistanceToDest(neighbor, dist);
      }
    }

    // TODO: use min-heap
    curr_offset = 0;
    int best_dist = kArrayGraphInfiniteDistance;

    for (const auto& vertex_id_and_offset : vertex_id_to_offset_) {
      ArrayGraphOffset offset = vertex_id_and_offset.second;

      if ((!IsVertexMarked(offset)) && best_dist > GetDistanceToDest(offset)) {
        curr_offset = offset;
        best_dist = GetDistanceToDest(offset);
      }
    }
  }

  UnmarkAllVertices();
}

// Helper struct used by OrderNeighborsByDistanceToDest.
struct NeighborData {
  net::GraphLinkIndex edge_index;
  ArrayGraphOffset neighbor_offset;
  int distance_to_destination;
  int distance_to_neighbor;
};

void ArrayGraph::OrderNeighborsByDistanceToDest(ArrayGraphOffset offset) {
  int num_neighbors = GetNeighborCount(offset);
  vector<NeighborData> neighbors(num_neighbors);
  for (int i = 0; i < num_neighbors; ++i) {
    NeighborData& data = neighbors[i];
    data.neighbor_offset = GetOffsetOfNeighbor(offset, i);
    data.distance_to_destination = GetDistanceToDest(data.neighbor_offset);
    data.edge_index = GetIndexOfEdge(offset, i);
    data.distance_to_neighbor = GetDistanceToNeighbor(offset, i);
  }

  std::sort(neighbors.begin(), neighbors.end(),
            [](const NeighborData& lhs, const NeighborData& rhs) {
              return lhs.distance_to_destination < rhs.distance_to_destination;
            });

  for (int i = 0; i < num_neighbors; ++i) {
    const NeighborData& data = neighbors[i];
    SetNeighbor(offset, i, data.edge_index, data.neighbor_offset,
                data.distance_to_neighbor);
  }
}

net::PBNet ToTree(const net::PBNet& graph, const std::string& root,
                  net::PathStorage* storage) {
  std::vector<net::GraphLinkIndex> edges_in_tree;
  auto array_graph = ArrayGraph::NewArrayGraph(graph, root, storage);
  ArrayGraphOffset root_offset = array_graph->dst_offset();

  std::deque<ArrayGraphOffset> stack;
  stack.push_back(root_offset);

  std::set<ArrayGraphOffset> visited_vertices;
  visited_vertices.insert(root_offset);

  while (!stack.empty()) {
    ArrayGraphOffset curr_vertex = stack.back();
    stack.pop_back();

    for (int i = 0; i < array_graph->GetNeighborCount(curr_vertex); i++) {
      ArrayGraphOffset neighbor =
          array_graph->GetOffsetOfNeighbor(curr_vertex, i);
      if (visited_vertices.count(neighbor)) {
        continue;
      }

      net::GraphLinkIndex edge_to_neighbor =
          array_graph->GetIndexOfEdge(curr_vertex, i);
      visited_vertices.insert(neighbor);

      edges_in_tree.push_back(edge_to_neighbor);
      stack.push_back(neighbor);
    }
  }

  net::PBNet return_graph(graph);
  return_graph.clear_links();

  for (net::GraphLinkIndex edge_index : edges_in_tree) {
    const net::GraphLink* edge = storage->GetLink(edge_index);
    *return_graph.add_links() = edge->ToProtobuf();
  }

  return return_graph;
}

}  // namespace dfs
}  // namespace ncode
