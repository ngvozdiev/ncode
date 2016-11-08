#include "algorithm.h"

#include <chrono>
#include <cstdbool>

#include "../common/perfect_hash.h"

namespace ncode {
namespace net {

constexpr Delay AllPairShortestPath::kMaxDistance;

SimpleDirectedGraph::SimpleDirectedGraph(const GraphStorage* parent)
    : graph_storage_(parent) {
  ConstructAdjacencyList();
}

void SimpleDirectedGraph::ConstructAdjacencyList() {
  for (GraphLinkIndex link : graph_storage_->AllLinks()) {
    const GraphLink* link_ptr = graph_storage_->GetLink(link);
    GraphNodeIndex src = link_ptr->src();
    GraphNodeIndex dst = link_ptr->dst();

    std::vector<net::GraphLinkIndex>& neighbors = adjacency_list_[src];
    for (net::GraphLinkIndex neighbor : neighbors) {
      const GraphLink* neighbor_ptr = graph_storage_->GetLink(neighbor);
      CHECK(neighbor_ptr->dst() != dst) << "Double edge in view";
    }

    neighbors.emplace_back(link);
  }
}

net::LinkSequence AllPairShortestPath::GetPath(GraphNodeIndex src,
                                               GraphNodeIndex dst) const {
  Delay dist = data_[src][dst].distance;
  if (dist == kMaxDistance) {
    return {};
  }

  Links links;
  GraphNodeIndex next = src;
  while (next != dst) {
    const SPData& datum = data_[next][dst];
    links.emplace_back(datum.next_link);
    next = datum.next_node;
  }

  return {links, dist};
}

Delay AllPairShortestPath::GetDistance(GraphNodeIndex src,
                                       GraphNodeIndex dst) const {
  return data_[src][dst].distance;
}

void AllPairShortestPath::ComputePaths() {
  const GraphStorage* graph_storage = graph_->graph_storage();

  const GraphNodeSet nodes = graph_storage->AllNodes();
  for (GraphNodeIndex node : nodes) {
    if (nodes_to_exclude_.Contains(node)) {
      continue;
    }

    data_[node][node].distance = Delay::zero();
  }

  for (GraphLinkIndex link : graph_storage->AllLinks()) {
    if (links_to_exclude_.Contains(link)) {
      continue;
    }

    const GraphLink* link_ptr = graph_storage->GetLink(link);
    Delay distance = link_ptr->delay();

    SPData& sp_data = data_[link_ptr->src()][link_ptr->dst()];
    sp_data.distance = distance;
    sp_data.next_link = link;
    sp_data.next_node = link_ptr->dst();
  }

  for (GraphNodeIndex k : nodes) {
    for (GraphNodeIndex i : nodes) {
      for (GraphNodeIndex j : nodes) {
        Delay i_k = data_[i][k].distance;
        Delay k_j = data_[k][j].distance;

        bool any_max = (i_k == kMaxDistance || k_j == kMaxDistance);
        Delay alt_distance = any_max ? kMaxDistance : i_k + k_j;

        SPData& i_j_data = data_[i][j];
        if (alt_distance < i_j_data.distance) {
          i_j_data.distance = alt_distance;

          const SPData& i_k_data = data_[i][k];
          i_j_data.next_link = i_k_data.next_link;
          i_j_data.next_node = i_k_data.next_node;
        }
      }
    }
  }
}

DFS::DFS(const SimpleDirectedGraph* graph, const GraphLinkSet& links_to_exclude,
         const GraphNodeSet& nodes_to_exclude)
    : graph_(graph),
      links_to_exclude_(links_to_exclude),
      nodes_to_exclude_(nodes_to_exclude),
      storage_(graph->graph_storage()),
      all_pair_sp_(graph_, links_to_exclude, nodes_to_exclude) {}

void DFS::Paths(GraphNodeIndex src, GraphNodeIndex dst, Delay max_distance,
                PathCallback path_callback) const {
  Delay total_distance = Delay::zero();
  GraphNodeSet nodes_seen;
  Links scratch_path;
  PathsRecursive(max_distance, src, dst, path_callback, &nodes_seen,
                 &scratch_path, &total_distance);
}

void DFS::PathsRecursive(Delay max_distance, GraphNodeIndex at,
                         GraphNodeIndex dst, PathCallback path_callback,
                         GraphNodeSet* nodes_seen, Links* current,
                         Delay* total_distance) const {
  if (at == dst) {
    path_callback(LinkSequence(*current, *total_distance));
    return;
  }

  Delay min_distance = all_pair_sp_.GetDistance(at, dst);
  min_distance += *total_distance;
  if (min_distance > max_distance) {
    return;
  }

  if (nodes_seen->Contains(at)) {
    return;
  }

  nodes_seen->Insert(at);

  const auto& adjacency_list = graph_->AdjacencyList();
  const std::vector<GraphLinkIndex>& outgoing_links = adjacency_list[at];

  for (GraphLinkIndex out_link : outgoing_links) {
    if (links_to_exclude_.Contains(out_link)) {
      continue;
    }

    const GraphLink* next_link = storage_->GetLink(out_link);
    GraphNodeIndex next_hop = next_link->dst();
    if (nodes_to_exclude_.Contains(next_hop)) {
      continue;
    }

    current->push_back(out_link);
    *total_distance += next_link->delay();
    PathsRecursive(max_distance, next_hop, dst, path_callback, nodes_seen,
                   current, total_distance);
    *total_distance -= next_link->delay();
    current->pop_back();
  }

  nodes_seen->Remove(at);
}

LinkSequence ShortestPath::GetPath(GraphNodeIndex dst) const {
  const GraphStorage* graph_storage = graph_->graph_storage();
  Links links_reverse;

  GraphNodeIndex current = dst;
  while (current != src_) {
    if (!previous_.HasValue(current)) {
      return {};
    }

    GraphLinkIndex link = previous_[current];
    const GraphLink* link_ptr = graph_storage->GetLink(link);

    links_reverse.emplace_back(link);
    current = link_ptr->src();
  }

  std::reverse(links_reverse.begin(), links_reverse.end());
  return LinkSequence(links_reverse, min_delays_[dst].distance);
}

void ShortestPath::ComputePaths() {
  std::set<std::pair<Delay, GraphNodeIndex>> vertex_queue;

  const GraphNodeMap<std::vector<GraphLinkIndex>>& adjacency_list =
      graph_->AdjacencyList();
  const GraphStorage* graph_storage = graph_->graph_storage();

  if (nodes_to_exclude_.Contains(src_)) {
    return;
  }

  min_delays_[src_].distance = Delay::zero();
  vertex_queue.emplace(Delay::zero(), src_);

  while (!vertex_queue.empty()) {
    auto it = vertex_queue.begin();

    Delay distance;
    GraphNodeIndex current;
    std::tie(distance, current) = *it;
    vertex_queue.erase(it);

    if (!adjacency_list.HasValue(current)) {
      continue;
    }

    const std::vector<GraphLinkIndex>& neighbors = adjacency_list[current];
    for (GraphLinkIndex out_link : neighbors) {
      if (links_to_exclude_.Contains(out_link)) {
        continue;
      }

      const GraphLink* out_link_ptr = graph_storage->GetLink(out_link);
      GraphNodeIndex neighbor_node = out_link_ptr->dst();
      if (nodes_to_exclude_.Contains(neighbor_node)) {
        continue;
      }

      Delay link_delay = out_link_ptr->delay();
      Delay distance_via_neighbor = distance + link_delay;
      Delay& curr_min_distance = min_delays_[neighbor_node].distance;

      if (distance_via_neighbor < curr_min_distance) {
        vertex_queue.erase({curr_min_distance, neighbor_node});
        curr_min_distance = distance_via_neighbor;
        previous_[neighbor_node] = out_link;
        vertex_queue.emplace(curr_min_distance, neighbor_node);
      }
    }
  }
}

}  // namespace net
}  // namespace ncode
