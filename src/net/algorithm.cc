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

// Returns a value to be used for all depreffed links. This value will be larger
// than the cost of any path through the graph.
static Delay DeprefCost(const SimpleDirectedGraph* graph) {
  const GraphStorage* storage = graph->graph_storage();
  GraphLinkSet all_links = storage->AllLinks();

  Delay total = Delay::zero();
  for (GraphLinkIndex link_index : all_links) {
    const GraphLink* link = storage->GetLink(link_index);
    total += link->delay();
  }

  return total;
}

GraphSearchAlgorithm::GraphSearchAlgorithm(const GraphSearchAlgorithmArgs& args,
                                           const SimpleDirectedGraph* graph)
    : graph_(graph),
      links_to_exclude_(args.links_to_exclude),
      nodes_to_exclude_(args.nodes_to_exclude) {}

DeprefSearchAlgorithm::DeprefSearchAlgorithm(
    const DeprefSearchAlgorithmArgs& args, const SimpleDirectedGraph* graph)
    : GraphSearchAlgorithm(args, graph),
      links_to_depref_(args.links_to_depref),
      nodes_to_depref_(args.nodes_to_depref),
      depref_cost_(DeprefCost(graph)) {}

net::LinkSequence AllPairShortestPath::GetPath(GraphNodeIndex src,
                                               GraphNodeIndex dst,
                                               bool* avoid_depref) const {
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

  if (dist >= depref_cost_) {
    dist = TotalDelayOfLinks(links, graph_->graph_storage());
    if (avoid_depref) {
      *avoid_depref = false;
    }
  }

  return {links, dist};
}

Delay AllPairShortestPath::GetDistance(GraphNodeIndex src, GraphNodeIndex dst,
                                       bool* avoid_depref) const {
  Delay distance = data_[src][dst].distance;
  if (distance == kMaxDistance) {
    return kMaxDistance;
  }

  if (distance >= depref_cost_) {
    distance = GetPath(src, dst).delay();
    if (avoid_depref) {
      *avoid_depref = false;
    }
  }

  return distance;
}

void AllPairShortestPath::ComputePaths() {
  const GraphStorage* graph_storage = graph_->graph_storage();

  const GraphNodeSet nodes = graph_storage->AllNodes();
  for (GraphNodeIndex node : nodes) {
    if (nodes_to_exclude_.Contains(node)) {
      continue;
    }

    SPData& node_data = data_[node][node];
    if (nodes_to_depref_.Contains(node)) {
      node_data.distance = depref_cost_;
    } else {
      node_data.distance = Delay::zero();
    }
  }

  for (GraphLinkIndex link : graph_storage->AllLinks()) {
    if (links_to_exclude_.Contains(link)) {
      continue;
    }

    const GraphLink* link_ptr = graph_storage->GetLink(link);
    Delay distance =
        links_to_depref_.Contains(link) ? depref_cost_ : link_ptr->delay();

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

static DeprefSearchAlgorithmArgs FromGraphArgs(
    const GraphSearchAlgorithmArgs& args) {
  DeprefSearchAlgorithmArgs out;
  out.links_to_exclude = args.links_to_exclude;
  out.nodes_to_exclude = args.nodes_to_exclude;
  return out;
}

DFS::DFS(const GraphSearchAlgorithmArgs& args, const SimpleDirectedGraph* graph)
    : GraphSearchAlgorithm(args, graph),
      storage_(graph->graph_storage()),
      all_pair_sp_(FromGraphArgs(args), graph_) {}

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
    PathsRecursive(max_distance, max_hops, next_hop, dst, path_callback,
                   nodes_seen, current, total_distance);
    *total_distance -= next_link->delay();
    current->pop_back();
  }

  nodes_seen->Remove(at);
}

LinkSequence ShortestPath::GetPath(GraphNodeIndex dst,
                                   bool* avoid_depref) const {
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
  if (distance >= depref_cost_) {
    distance = TotalDelayOfLinks(links_reverse, graph_->graph_storage());
    if (avoid_depref) {
      *avoid_depref = false;
    }
  }

  return {links_reverse, distance};
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

      bool depref = links_to_depref_.Contains(out_link) ||
                    nodes_to_depref_.Contains(neighbor_node);
      Delay link_delay = depref ? depref_cost_ : out_link_ptr->delay();
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

KShortestPaths::KShortestPaths(const DeprefSearchAlgorithmArgs& args,
                               GraphNodeIndex src, GraphNodeIndex dst,
                               const SimpleDirectedGraph* graph)
    : DeprefSearchAlgorithm(args, graph), src_(src), dst_(dst) {}

DeprefSearchAlgorithmArgs KShortestPaths::GetArgs() const {
  DeprefSearchAlgorithmArgs args;
  args.links_to_exclude = links_to_exclude_;
  args.links_to_depref = links_to_depref_;
  args.nodes_to_exclude = nodes_to_exclude_;
  args.nodes_to_depref = nodes_to_depref_;

  return args;
}

LinkSequence KShortestPaths::NextPath() {
  const GraphStorage* graph_storage = graph_->graph_storage();
  if (k_paths_.empty()) {
    ShortestPath sp(GetArgs(), src_, graph_);
    LinkSequence path = sp.GetPath(dst_, nullptr);
    k_paths_.emplace_back(path);
    return path;
  }

  const LinkSequence& last_path = k_paths_.back();
  const Links& last_path_links = last_path.links();

  LOG(ERROR) << "Last path " << last_path.ToString(graph_storage);

  GraphNodeSet node_exclusion_set;
  Links root_path;
  for (GraphLinkIndex link_index : last_path_links) {
    const GraphLink* link = graph_storage->GetLink(link_index);
    GraphNodeIndex spur_node = link->src();
    GraphLinkSet link_exclusion_set = GetLinkExclusionSet(root_path);

    LOG(ERROR) << "Spur " << graph_storage->GetNode(spur_node)->id();

    DeprefSearchAlgorithmArgs args = GetArgs();
    args.links_to_exclude.InsertAll(link_exclusion_set);
    args.nodes_to_exclude.InsertAll(node_exclusion_set);
    ShortestPath sp(args, spur_node, graph_);

    LinkSequence spur_path = sp.GetPath(dst_, nullptr);
    if (!spur_path.empty()) {
      LOG(ERROR) << "Spur path " << spur_path.ToString(graph_storage);
      const Links& spur_path_links = spur_path.links();

      Links candidate_links = root_path;
      candidate_links.insert(candidate_links.end(), spur_path_links.begin(),
                             spur_path_links.end());
      LinkSequence candidate_path(
          candidate_links, TotalDelayOfLinks(candidate_links, graph_storage));
      candidates_.push(candidate_path);
    }

    node_exclusion_set.Insert(spur_node);
    root_path.emplace_back(link_index);
  }

  if (candidates_.empty()) {
    return {};
  }

  LinkSequence min_candidate = candidates_.top();
  candidates_.pop();
  k_paths_.emplace_back(min_candidate);
  return min_candidate;
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

GraphLinkSet KShortestPaths::GetLinkExclusionSet(const Links& root_path) {
  GraphLinkSet out;
  const GraphStorage* storage = graph_->graph_storage();
  for (const LinkSequence& k_path : k_paths_) {
    if (k_path.size() < root_path.size()) {
      continue;
    }

    const Links& k_path_links = k_path.links();
    if (HasPrefix(k_path_links, root_path)) {
      CHECK(k_path_links.size() > root_path.size());
      LOG(ERROR) << "To exclude "
                 << storage->GetLink(k_path_links[root_path.size()])->ToString()
                 << " root path " << root_path.size() << " kpl "
                 << k_path_links.size();
      out.Insert(k_path_links[root_path.size()]);
    }
  }

  return out;
}

}  // namespace net
}  // namespace ncode
