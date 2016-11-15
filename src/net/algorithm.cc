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

GraphSearchAlgorithm::GraphSearchAlgorithm(
    const GraphSearchAlgorithmConfig& config, const SimpleDirectedGraph* graph)
    : graph_(graph), config_(config) {}

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
    if (config_.CanExcludeNode(node)) {
      continue;
    }

    SPData& node_data = data_[node][node];
    node_data.distance = Delay::zero();
  }

  for (GraphLinkIndex link : graph_storage->AllLinks()) {
    if (config_.CanExcludeLink(link)) {
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

DFS::DFS(const GraphSearchAlgorithmConfig& config,
         const SimpleDirectedGraph* graph)
    : GraphSearchAlgorithm(config, graph),
      storage_(graph->graph_storage()),
      all_pair_sp_(config, graph_) {}

void DFS::Paths(GraphNodeIndex src, GraphNodeIndex dst, Delay max_distance,
                size_t max_hops, PathCallback path_callback) const {
  Delay total_distance = Delay::zero();
  GraphNodeSet nodes_seen;
  Links scratch_path;
  PathsRecursive(max_distance, max_hops, src, dst, path_callback, &nodes_seen,
                 &scratch_path, &total_distance);
}

void DFS::PathsRecursive(Delay max_distance, size_t max_hops, GraphNodeIndex at,
                         GraphNodeIndex dst, PathCallback path_callback,
                         GraphNodeSet* nodes_seen, Links* current,
                         Delay* total_distance) const {
  if (current->size() > max_hops) {
    return;
  }

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
    if (config_.CanExcludeLink(out_link)) {
      continue;
    }

    const GraphLink* next_link = storage_->GetLink(out_link);
    GraphNodeIndex next_hop = next_link->dst();
    if (config_.CanExcludeNode(next_hop)) {
      continue;
    }

    current->push_back(out_link);
    *total_distance += next_link->delay();
    PathsRecursive(max_distance, max_hops, next_hop, dst, path_callback,
                   nodes_seen, current, total_distance);
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
  Delay distance = min_delays_[dst].distance;
  return {links_reverse, distance};
}

void ShortestPath::ComputePaths() {
  using DelayAndIndex = std::pair<Delay, GraphNodeIndex>;
  std::priority_queue<DelayAndIndex, std::vector<DelayAndIndex>,
                      std::greater<DelayAndIndex>> vertex_queue;

  const GraphNodeMap<std::vector<GraphLinkIndex>>& adjacency_list =
      graph_->AdjacencyList();
  const GraphStorage* graph_storage = graph_->graph_storage();
  if (config_.CanExcludeNode(src_)) {
    return;
  }

  min_delays_[src_].distance = Delay::zero();
  vertex_queue.emplace(Delay::zero(), src_);

  while (!vertex_queue.empty()) {
    Delay distance;
    GraphNodeIndex current;
    std::tie(distance, current) = vertex_queue.top();
    vertex_queue.pop();

    if (!adjacency_list.HasValue(current)) {
      // A leaf.
      continue;
    }

    if (distance > min_delays_[current].distance) {
      // Bogus leftover node, since we never delete nodes from the heap.
      continue;
    }

    const std::vector<GraphLinkIndex>& neighbors = adjacency_list[current];
    for (GraphLinkIndex out_link : neighbors) {
      if (config_.CanExcludeLink(out_link)) {
        continue;
      }

      const GraphLink* out_link_ptr = graph_storage->GetLink(out_link);
      GraphNodeIndex neighbor_node = out_link_ptr->dst();
      if (config_.CanExcludeNode(neighbor_node)) {
        continue;
      }

      Delay link_delay = out_link_ptr->delay();
      Delay distance_via_neighbor = distance + link_delay;
      Delay& curr_min_distance = min_delays_[neighbor_node].distance;

      if (distance_via_neighbor < curr_min_distance) {
        curr_min_distance = distance_via_neighbor;
        previous_[neighbor_node] = out_link;
        vertex_queue.emplace(curr_min_distance, neighbor_node);
      }
    }
  }
}

static void AddFromPath(const SimpleDirectedGraph& graph,
                        const LinkSequence& path, Links* out,
                        GraphNodeSet* nodes) {
  const GraphStorage* graph_storage = graph.graph_storage();
  for (GraphLinkIndex link_in_path : path.links()) {
    const GraphLink* link_ptr = graph_storage->GetLink(link_in_path);

    out->emplace_back(link_in_path);
    nodes->Insert(link_ptr->src());
    nodes->Insert(link_ptr->dst());
  }
}

LinkSequence WaypointShortestPath(const GraphSearchAlgorithmConfig& config,
                                  const Links& waypoints, GraphNodeIndex src,
                                  GraphNodeIndex dst,
                                  const SimpleDirectedGraph* graph) {
  CHECK(src != dst);
  const GraphStorage* graph_storage = graph->graph_storage();

  // As new paths are discovered nodes will be added to this set to make sure
  // the next paths do not include any nodes from previous paths.
  GraphSearchAlgorithmConfig config_copy = config;
  GraphNodeSet nodes_to_exclude;
  config_copy.AddToExcludeNodes(&nodes_to_exclude);

  // The shortest path is the combination of the shortest paths between the
  // nodes that we have to visit.
  GraphNodeIndex current_point = src;
  Links path;
  Delay total_delay = Delay::zero();
  for (GraphLinkIndex link : waypoints) {
    // The next SP is that between the current point and the source of the
    // edge, the path is then concatenated with the edge and the current point
    // is set to the end of the edge.
    const GraphLink* link_ptr = graph_storage->GetLink(link);

    if (src != link_ptr->src()) {
      ShortestPath sp(config_copy, current_point, graph);
      LinkSequence pathlet = sp.GetPath(link_ptr->src());
      if (pathlet.empty()) {
        return {};
      }

      AddFromPath(*graph, pathlet, &path, &nodes_to_exclude);
      total_delay += pathlet.delay();
    }

    path.emplace_back(link);
    total_delay += link_ptr->delay();
    current_point = link_ptr->dst();
  }

  if (current_point != dst) {
    // Have to connect the last hop with the destination.
    ShortestPath sp(config_copy, current_point, graph);
    LinkSequence pathlet = sp.GetPath(dst);
    if (pathlet.empty()) {
      return {};
    }

    AddFromPath(*graph, pathlet, &path, &nodes_to_exclude);
    total_delay += pathlet.delay();
  }

  return LinkSequence(path, total_delay);
}

KShortestPaths::KShortestPaths(const GraphSearchAlgorithmConfig& config,
                               const std::vector<GraphLinkIndex>& waypoints,
                               GraphNodeIndex src, GraphNodeIndex dst,
                               const SimpleDirectedGraph* graph)
    : GraphSearchAlgorithm(config, graph),
      waypoints_(waypoints),
      src_(src),
      dst_(dst) {}

LinkSequence KShortestPaths::NextPath() {
  const GraphStorage* graph_storage = graph_->graph_storage();
  if (k_paths_.empty()) {
    LinkSequence path =
        WaypointShortestPath(config_, waypoints_, src_, dst_, graph_);
    k_paths_.emplace_back(path, 0);
    return path;
  }

  const PathAndStartIndex& last_path_and_start_index = k_paths_.back();
  const LinkSequence& last_path = last_path_and_start_index.first;
  const Links& last_path_links = last_path.links();
  size_t start_index = last_path_and_start_index.second;

  GraphSearchAlgorithmConfig config_copy = config_;
  GraphLinkSet links_to_exclude;
  GraphNodeSet nodes_to_exclude;
  config_copy.AddToExcludeLinks(&links_to_exclude);
  config_copy.AddToExcludeNodes(&nodes_to_exclude);

  Links root_path;
  for (size_t i = 0; i < last_path_links.size(); ++i) {
    GraphLinkIndex link_index = last_path_links[i];
    const GraphLink* link = graph_storage->GetLink(link_index);
    GraphNodeIndex spur_node = link->src();
    if (i < start_index) {
      nodes_to_exclude.Insert(spur_node);
      root_path.emplace_back(link_index);
      continue;
    }

    GetLinkExclusionSet(root_path, &links_to_exclude);
    LinkSequence spur_path =
        WaypointShortestPath(config_copy, waypoints_, spur_node, dst_, graph_);
    if (!spur_path.empty()) {
      const Links& spur_path_links = spur_path.links();

      Links candidate_links = root_path;
      candidate_links.insert(candidate_links.end(), spur_path_links.begin(),
                             spur_path_links.end());
      LinkSequence candidate_path(
          candidate_links, TotalDelayOfLinks(candidate_links, graph_storage));
      candidates_.emplace(candidate_path, i);
    }

    links_to_exclude.Clear();
    nodes_to_exclude.Insert(spur_node);
    root_path.emplace_back(link_index);
  }

  if (candidates_.empty()) {
    return {};
  }

  PathAndStartIndex min_candidate = candidates_.top();
  candidates_.pop();
  k_paths_.emplace_back(min_candidate);
  return min_candidate.first;
}

bool KShortestPaths::HasPrefix(const Links& path, const Links& prefix) {
  CHECK(prefix.size() <= path.size()) << prefix.size() << " vs " << path.size();
  for (size_t i = 0; i < prefix.size(); ++i) {
    if (path[i] != prefix[i]) {
      return false;
    }
  }

  return true;
}

void KShortestPaths::GetLinkExclusionSet(const Links& root_path,
                                         GraphLinkSet* out) {
  for (const PathAndStartIndex& k_path_and_start_index : k_paths_) {
    const LinkSequence& k_path = k_path_and_start_index.first;
    if (k_path.size() < root_path.size()) {
      continue;
    }

    const Links& k_path_links = k_path.links();
    if (HasPrefix(k_path_links, root_path)) {
      CHECK(k_path_links.size() > root_path.size());
      out->Insert(k_path_links[root_path.size()]);
    }
  }
}

}  // namespace net
}  // namespace ncode
