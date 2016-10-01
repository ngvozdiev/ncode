#include <thread>
#include <chrono>

#include "dfs_session.h"

namespace ncode {
namespace dfs {

// Called when a path has been found.
bool DFSSession::PathFound(const net::LinkSequence& new_path,
                           const Constraint& request_constraint,
                           std::vector<const net::GraphPath*>* paths_found,
                           uint32_t max_paths) {
  if (paths_found->size() == max_paths) {
    return false;
  }

  // Need to check if it meets the session constraints.
  if (!session_wide_constraint_->PathComplies(new_path)) {
    return true;
  }

  // And if it meets the request's constraints.
  if (!request_constraint.PathComplies(new_path)) {
    return true;
  }

  paths_found->push_back(
      array_graph_->storage()->PathFromLinks(new_path, cookie_));
  return true;
}

void DFSSession::AddSessionWideConstraint(
    std::unique_ptr<Constraint> new_constraint) {
  auto and_constraint = std::unique_ptr<Constraint>(new AndConstraint(
      std::move(session_wide_constraint_), std::move(new_constraint)));
  session_wide_constraint_ = std::move(and_constraint);
}

std::vector<const net::GraphPath*> DFSSession::ProcessRequestOrThrow(
    const PBDFSRequest& request, uint32_t max_paths,
    PBConstraint request_constraint) {
  std::unique_ptr<Constraint> compiled_constraint =
      CompileConstraint(request_constraint, array_graph_->storage());

  std::vector<const net::GraphPath*> return_vector;

  {
    std::unique_lock<std::mutex> lock(mu_);
    assert(!to_kill_ && "Session already killed");

    assert(current_dfs_.get() == nullptr &&
           "Another request in progress in the same session. Calls need to "
           "be synchronous.");

    current_dfs_ = make_unique<DFS>(
        request, *array_graph_, [this, &return_vector, &compiled_constraint,
                                 max_paths](const net::LinkSequence& path) {
          return PathFound(path, *compiled_constraint, &return_vector,
                           max_paths);
        });
  }

  current_dfs_->Search();

  std::unique_lock<std::mutex> lock(mu_);
  current_dfs_.release();
  return return_vector;
}

void DFSSession::TerminateSession() {
  std::unique_lock<std::mutex> lock(mu_);
  to_kill_ = true;

  if (current_dfs_.get() != nullptr) {
    current_dfs_->Terminate();
  }
}

}  // namespace dfs
}  // namepsace ncode
