#include "constraint.h"

#include <chrono>
#include <functional>
#include <initializer_list>

#include "../common/logging.h"
#include "../common/perfect_hash.h"
#include "../common/strutil.h"
#include "../common/substitute.h"

namespace ncode {
namespace net {

Conjunction::Conjunction(const GraphLinkSet& to_avoid,
                         const std::vector<GraphLinkIndex>& to_visit)
    : to_avoid_(to_avoid), to_visit_(to_visit) {}

bool Conjunction::PathComplies(const net::LinkSequence& link_sequence) const {
  size_t visited_index = 0;
  for (GraphLinkIndex link : link_sequence.links()) {
    if (to_avoid_.Contains(link)) {
      return false;
    }

    if (to_visit_.size() > visited_index) {
      if (link == to_visit_[visited_index]) {
        ++visited_index;
      }
    }
  }

  return visited_index == to_visit_.size();
}

void Conjunction::AddFromPath(const SimpleDirectedGraph& graph,
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

net::LinkSequence Conjunction::ShortestCompliantPath(
    const SimpleDirectedGraph& graph, const GraphLinkSet& to_avoid,
    GraphNodeIndex src, GraphNodeIndex dst) const {
  CHECK(src != dst);

  // Will combine the links that this conjunction should avoid (in to_avoid_)
  // with the ones that the call wants to avoid (in to_avoid).
  GraphLinkSet links_to_avoid = to_avoid_;
  links_to_avoid.InsertAll(to_avoid);

  // As new paths are discovered nodes will be added to this set to make sure
  // the next paths do not include any nodes from previous paths.
  GraphNodeSet nodes_to_avoid;

  // The shortest path is the combination of the shortest paths between the
  // nodes that we have to visit.
  GraphNodeIndex current_point = src;
  Links path;
  Delay total_delay = Delay::zero();
  for (GraphLinkIndex link : to_visit_) {
    // The next SP is that between the current point and the source of the
    // edge, the path is then concatenated with the edge and the current point
    // is set to the end of the edge.
    const GraphLink* link_ptr = graph.graph_storage()->GetLink(link);

    if (src != link_ptr->src()) {
      ShortestPath sp(current_point, &graph, links_to_avoid, nodes_to_avoid);
      LinkSequence pathlet = sp.GetPath(link_ptr->src());
      if (pathlet.empty()) {
        return {};
      }

      AddFromPath(graph, pathlet, &path, &nodes_to_avoid);
      total_delay += pathlet.delay();
    }

    path.emplace_back(link);
    total_delay += link_ptr->delay();
    current_point = link_ptr->dst();
  }

  if (current_point != dst) {
    // Have to connect the last hop with the destination.
    ShortestPath sp(current_point, &graph, links_to_avoid, nodes_to_avoid);
    LinkSequence pathlet = sp.GetPath(dst);
    if (pathlet.empty()) {
      return {};
    }

    AddFromPath(graph, pathlet, &path, &nodes_to_avoid);
    total_delay += pathlet.delay();
  }

  return LinkSequence(path, total_delay);
}

std::string Conjunction::ToString(const net::GraphStorage* storage) const {
  std::string to_avoid_str;
  std::string to_visit_str;

  Join(to_avoid_.begin(), to_avoid_.end(), ",", [storage](GraphLinkIndex link) {
    return storage->GetLink(link)->ToString();
  }, &to_avoid_str);

  Join(to_visit_.begin(), to_visit_.end(), ",", [storage](GraphLinkIndex link) {
    return storage->GetLink(link)->ToString();
  }, &to_avoid_str);

  return Substitute("(AVOID $0, VISIT $1)", to_avoid_str, to_visit_str);
}

bool Disjunction::PathComplies(const net::LinkSequence& link_sequence) const {
  // The path complies if any of the conjunctions comply.
  for (const auto& conjunction : conjunctions_) {
    if (conjunction->PathComplies(link_sequence)) {
      return true;
    }
  }

  return false;
}

net::LinkSequence Disjunction::ShortestCompliantPath(
    const SimpleDirectedGraph& graph, const GraphLinkSet& to_avoid,
    GraphNodeIndex src, GraphNodeIndex dst) const {
  // To produce the shortest compliant path we have to consider the shortest
  // paths of all conjunctions.
  net::LinkSequence candidate;
  for (const auto& conjunction : conjunctions_) {
    net::LinkSequence conjunction_sp =
        conjunction->ShortestCompliantPath(graph, to_avoid, src, dst);
    if (!conjunction_sp.empty() && conjunction_sp.delay() < candidate.delay()) {
      candidate = conjunction_sp;
    }
  }

  return candidate;
}

std::string Disjunction::ToString(const net::GraphStorage* storage) const {
  std::string out;
  Join(conjunctions_.begin(), conjunctions_.end(),
       " OR ", [storage](const std::unique_ptr<Conjunction>& cj) {
         return cj->ToString(storage);
       }, &out);
  return out;
}

bool DummyConstraint::PathComplies(
    const net::LinkSequence& link_sequence) const {
  Unused(link_sequence);
  return true;
}

net::LinkSequence DummyConstraint::ShortestCompliantPath(
    const SimpleDirectedGraph& graph, const GraphLinkSet& to_avoid,
    GraphNodeIndex src, GraphNodeIndex dst) const {
  AllPairShortestPath sp(&graph, to_avoid);
  return sp.GetPath(src, dst);
}

std::string DummyConstraint::ToString(const net::GraphStorage* storage) const {
  Unused(storage);
  return "[DUMMY]";
}

}  // namespace net
}  // namespace ncode
