#ifndef NCODE_DFS_H
#define NCODE_DFS_H

#include <stddef.h>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <vector>

#include "../net/net_common.h"
#include "array_graph.h"

namespace ncode {
namespace dfs {
class PBDFSRequest;
} /* namespace dfs */
} /* namespace ncode */

namespace ncode {
namespace dfs {
// A new path through the graph has been found. The return value can be used as
// an indicator whether to continue the search or to terminate it.
typedef std::function<bool(const net::LinkSequence&)> PathFoundCallback;

// A stack frame stores the state at a single hop of the DFS
struct DFSStackFrame {
  DFSStackFrame()
      : neighbor_index(-1), edge_weight(1), edge_index(-1), vertex_offset(-1) {}

  int neighbor_index;  // The index of the neighbor that was visited during this
                       // step of the DFS.
  uint32_t edge_weight;
  EdgeIndex edge_index;            // The index of the edge.
  ArrayGraphOffset vertex_offset;  // The offset of the vertex at this hop.
};

// A class that can discover all paths between a source and a destination in a
// graph. Path is defined as a sequence of unique edges.
class DFS {
 public:
  // Constructs a new DFS or throws an exception if the request is malformed
  // (e.g. the source is missing or it is not present in the graph)
  DFS(const PBDFSRequest& dfs_request, const ArrayGraph& array_graph,
      PathFoundCallback path_found_callback);

  // Performs a DFS starting at the source in the request. There are two ways
  // the DFS can terminate - it can discover all paths that satisfy paths length
  // and hop length constraints or it can be prematurely terminated by returning
  // false from path_found_callback_.
  void Search();

  // Teminates the search.
  void Terminate() { to_kill_ = true; }

 private:
  // Called when the DFS jumps to a new vertex. Will update the next entry in
  // the stack.
  void PushNewStackFrame(ArrayGraphOffset vertex_offset, int edge_index,
                         int edge_wieght) {
    DFSStackFrame* sf = &stack_[++frame_pointer_];
    sf->vertex_offset = vertex_offset;
    sf->neighbor_index = -1;
    sf->edge_index = edge_index;
    sf->edge_weight = edge_wieght;
  }

  // Returns the frame at the top of the stack.
  DFSStackFrame* PeekStackFrame() { return &stack_[frame_pointer_]; }

  // Returns true if the stack is not empty.
  bool StackIsNotEmpty() { return frame_pointer_ != -1; }

  // Decrements the frame pointer.
  void PopStackFrame() { frame_pointer_--; }

  // Performs DFS, starting at the element at the top of the current stack.
  void SearchFromTopOfStack();

  // Called when a path has been found. This function creates a new path from
  // the elements in the stack and passes it to the callback.
  bool PathFound();

  void MarkNode(ArrayGraphOffset node) {
    marked_nodes_.resize(
        std::max(marked_nodes_.size(), static_cast<size_t>(node) + 1));
    marked_nodes_[node] = true;
  }

  void UnmarkNode(ArrayGraphOffset node) {
    marked_nodes_.resize(
        std::max(marked_nodes_.size(), static_cast<size_t>(node) + 1));
    marked_nodes_[node] = false;
  }

  bool IsNodeMarked(ArrayGraphOffset node) {
    marked_nodes_.resize(
        std::max(marked_nodes_.size(), static_cast<size_t>(node) + 1));
    return marked_nodes_[node];
  }

  // The maximum number of hops that the DFS can extend to. Paths that have more
  // hops than that are not going to be discovered.
  const uint32_t max_depth_hops_;

  // The maximum depth of paths in terms of cumulative edge weight.
  const uint32_t max_depth_weight_;

  // The maximum number of milliseconds for the search to run.
  const uint32_t max_duration_ms_;

  // Checking whether it is time to stop the DFS (either because it has taken
  // more time than max_duration_ms_ or because it was explicitly killed via
  // Terminate) can be potentially expensive so we will sacrifice precision and
  // only do it every steps_to_check_for_stop_ steps.
  const uint32_t steps_to_check_for_stop_;

  // The offset of the destination.
  const ArrayGraphOffset dst_offset_;

  // Callback to call when a new path is found.
  const PathFoundCallback path_found_callback_;

  // The offset of the source.
  ArrayGraphOffset src_offset_;

  // A stack that can hold at most max_depth_hops_ elements. No elements are
  // popped or pushed in the traditional sense - an index into the stack is
  // moved and the value of old elements (those above the index) will be
  // overwritten by subsequent operations.
  std::vector<DFSStackFrame> stack_;

  // Index into stack_.
  int frame_pointer_;

  // A set of markings for edges. The array graph provides a linear indexing of
  // edges (each edge has an index in the range [0, num_edges)) that is used to
  // treat the vector as a set.
  std::vector<bool> marked_edges_;

  // A similar to marked_edges_, but for nodes.
  std::vector<bool> marked_nodes_;

  // Whether or not to avoid paths that have the same node more than once.
  bool node_disjoint_;

  // The graph that DFS will run on
  const ArrayGraph* graph_;

  // Whether explicit termination was requested.
  std::atomic<bool> to_kill_;
};

}  // namespace dfs
}  // namespace ncode

#endif /* NCODE_DFS_H */
