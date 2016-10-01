#ifndef PATHFINDER_SESSION_H
#define PATHFINDER_SESSION_H

#include <functional>
#include <atomic>
#include <condition_variable>
#include <list>
#include <memory>
#include <mutex>
#include <set>
#include <thread>

#include "../common/common.h"
#include "constraint.h"
#include "dfs.h"

namespace ncode {
namespace dfs {

// A thin wrapper around DFS. Stores the graph and enforces the required limit
// on the number of paths per request.
class DFSSession {
 public:
  DFSSession(const net::PBNet& graph, const std::string& dst,
             const PBConstraint& constraint, uint64_t cookie,
             net::PathStorage* storage)
      : cookie_(cookie), to_kill_(false) {
    array_graph_ = ArrayGraph::NewArrayGraph(graph, dst, storage);
    session_wide_constraint_ = CompileConstraint(constraint, storage);
  }

  ~DFSSession() {
    // Need to explicitly kill the session here, since if the main thread is
    // joinable its destructor will abort execution.
    TerminateSession();
  }

  // Constructs a new DFS instance and runs it. This method will block - the
  // API that this class exposes is synchronous.
  std::vector<const net::GraphPath*> ProcessRequestOrThrow(
      const PBDFSRequest& request, uint32_t max_paths,
      PBConstraint request_constraint);

  // A version of ProcessRequest that takes a dummy constraint. Useful for
  // testing.
  std::vector<const net::GraphPath*> ProcessRequestOrThrow(
      const PBDFSRequest& request, uint32_t max_paths) {
    PBConstraint dummy_constraint;
    dummy_constraint.set_type(PBConstraint::DUMMY);
    return ProcessRequestOrThrow(request, max_paths, dummy_constraint);
  }

  // A version of ProcessRequest that takes a dummy constraint and imposes no
  // restriction on the max number of paths discovered. Useful for testing.
  std::vector<const net::GraphPath*> ProcessRequestOrThrow(
      const PBDFSRequest& request) {
    return ProcessRequestOrThrow(request, std::numeric_limits<uint32_t>::max());
  }

  // Will terminate the session by killing the currently running DFS (if any)
  void TerminateSession();

  // Adds a new session-wide constraint. More precisely, replaces the current
  // constraint with an AND(current_constraint, new_constraint).
  void AddSessionWideConstraint(std::unique_ptr<Constraint> new_constraint);

 private:
  // Called when a path has been found. The first argument is the new path, the
  // second is the list of paths in this request. The third is the maximum
  // number of paths to return.
  bool PathFound(const net::LinkSequence& new_path,
                 const Constraint& request_constraint,
                 std::vector<const net::GraphPath*>* paths_found,
                 uint32_t max_paths);

  // All paths in this session will belong to the aggregate with this cookie
  // value.
  uint64_t cookie_;

  // A "compiled" immutable form of the graph that is used by DFS instances.
  std::unique_ptr<ArrayGraph> array_graph_;

  // The most recent DFS instance. Each DFS request is executed in a separate
  // DFS instance.
  std::unique_ptr<DFS> current_dfs_;

  // A mutex to protect current_dfs_
  std::mutex mu_;

  // Set to true iff TerminateSession has been called. Protected by mu_.
  bool to_kill_;

  // The session-wide constraint. Every path returned should meet this.
  std::unique_ptr<Constraint> session_wide_constraint_;

  DISALLOW_COPY_AND_ASSIGN(DFSSession);
};

}  // namespace dfs
}  // namespace ncode

#endif /* PATHFINDER_SESSION_H */
