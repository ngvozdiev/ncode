#include "dfs.h"

#include <chrono>
#include <map>
#include <stdexcept>
#include <utility>

#include "dfs.pb.h"

namespace ncode {
namespace dfs {

DFS::DFS(const PBDFSRequest& dfs_request, const ArrayGraph& array_graph,
         PathFoundCallback path_found_callback)
    : max_depth_hops_(dfs_request.max_depth_hops()),
      max_depth_weight_(dfs_request.max_depth_metric()),
      max_duration_ms_(dfs_request.max_duration_ms()),
      steps_to_check_for_stop_(dfs_request.steps_to_check_for_stop()),
      dst_offset_(array_graph.dst_offset()),
      path_found_callback_(path_found_callback),
      frame_pointer_(-1),
      node_disjoint_(dfs_request.node_disjoint()),
      graph_(&array_graph),
      to_kill_(false) {
  const auto& vertex_id_to_offset = array_graph.vertex_id_to_offset();

  const auto& it = vertex_id_to_offset.find(dfs_request.src());
  CHECK(it != vertex_id_to_offset.end()) << "Source vertex not found";
  CHECK(steps_to_check_for_stop_)
      << "Steps to check for stop should be positive";
  CHECK(max_duration_ms_) << "Max duration ms should be positive";

  src_offset_ = it->second;
  stack_.resize(max_depth_hops_ + 1);  // Need to provision for n + 1 nodes
                                       // where n is the max number of edges
                                       // in the path.
  marked_edges_.resize(graph_->edge_index_to_edge().size());
}

bool DFS::PathFound() {
  int len = frame_pointer_;
  std::vector<const net::GraphLink*> links(len);

  const std::vector<const net::GraphLink*>& edge_index_to_edge =
      graph_->edge_index_to_edge();

  for (int i = 0; i < len; ++i) {
    DFSStackFrame* frame = &stack_[i + 1];

    EdgeIndex edge_index = frame->edge_index;
    links[i] = edge_index_to_edge[edge_index];
  }

  return path_found_callback_(links);
}

void DFS::SearchFromTopOfStack() {
  int cutoff = max_depth_hops_;
  uint32_t total_weight_so_far = 0;
  uint32_t steps_so_far = 0;
  auto search_start_time = std::chrono::system_clock::now();

  while (StackIsNotEmpty()) {
    steps_so_far++;
    if (steps_so_far == steps_to_check_for_stop_) {
      steps_so_far = 0;

      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now() - search_start_time);

      if (to_kill_ || duration.count() > max_duration_ms_) {
        break;
      }
    }

    DFSStackFrame* curr_frame = PeekStackFrame();
    ArrayGraphOffset curr_vertex = curr_frame->vertex_offset;

    if (curr_vertex == dst_offset_) {
      if (!PathFound()) {
        break;
      }

      if (curr_frame->edge_index >= 0) {
        marked_edges_[curr_frame->edge_index] = false;
        if (node_disjoint_) {
          UnmarkNode(curr_frame->vertex_offset);
        }
        total_weight_so_far -= curr_frame->edge_weight;
      }

      PopStackFrame();
      continue;
    }

    int next_neighbor_index = curr_frame->neighbor_index + 1;
    int neighbor_count = graph_->GetNeighborCount(curr_vertex);
    if (next_neighbor_index == neighbor_count) {
      if (curr_frame->edge_index >= 0) {
        marked_edges_[curr_frame->edge_index] = false;
        if (node_disjoint_) {
          UnmarkNode(curr_frame->vertex_offset);
        }
        total_weight_so_far -= curr_frame->edge_weight;
      }

      PopStackFrame();
      continue;
    }

    curr_frame->neighbor_index = next_neighbor_index;

    // The offset of the next neighbor
    int neighbor_offset =
        graph_->GetOffsetOfNeighbor(curr_vertex, next_neighbor_index);

    // The index of the edge leading to the next neighbor
    EdgeIndex edge_to_neighbor =
        graph_->GetIndexOfEdge(curr_frame->vertex_offset, next_neighbor_index);

    // The weight of the edge leading to the next neighbor
    int weight_of_edge =
        graph_->GetDistanceToNeighbor(curr_vertex, next_neighbor_index);

    // Minimum distance in terms of weight from the next neighbor to the
    // destination
    uint32_t min_dist_to_dest = graph_->GetDistanceToDest(neighbor_offset);

    // The edge that we are about to take has already been seen - taking it
    // would result in a loop.
    bool prune_loop = marked_edges_[edge_to_neighbor];
    if (!prune_loop && node_disjoint_) {
      prune_loop = IsNodeMarked(neighbor_offset);
    }

    // Can we hope to get to the destination within our weight budget by going
    // to that neighbor.
    bool prune_too_far =
        (total_weight_so_far + min_dist_to_dest) > max_depth_weight_;

    bool prune_too_many_hops = (frame_pointer_ == cutoff);
    if (prune_loop || prune_too_far || prune_too_many_hops) {
      continue;
    }

    marked_edges_[edge_to_neighbor] = true;
    if (node_disjoint_) {
      MarkNode(neighbor_offset);
    }

    total_weight_so_far += weight_of_edge;

    // Prune if we have gone too far.
    if (total_weight_so_far > max_depth_weight_) {
      total_weight_so_far -= weight_of_edge;
      continue;
    }

    //    std::cout << "Push w " << total_weight_so_far << "\n";
    PushNewStackFrame(neighbor_offset, edge_to_neighbor, weight_of_edge);
  }
}

void DFS::Search() {
  if (node_disjoint_) {
    MarkNode(src_offset_);
  }

  PushNewStackFrame(src_offset_, -1, -1);
  SearchFromTopOfStack();

  if (node_disjoint_) {
    UnmarkNode(src_offset_);
  }
}

}  // namespace dfs
}  // namespace ncode
