#ifndef NCODE_COMMON_THREAD_RUNNER_H
#define NCODE_COMMON_THREAD_RUNNER_H

#include <stddef.h>
#include <functional>
#include <thread>
#include <vector>

namespace ncode {

// Runs instances of a given function in parallel. At any given moment in time
// up to 'batch_size' function will run in parallel. This function will block
// and return when all functions have completed.
template <typename T>
void RunInParallel(const std::vector<T>& arguments,
                   std::function<void(const T&)> f, size_t batch_size = 4) {
  CHECK(batch_size > 0) << "Zero batch size";

  std::vector<std::thread> threads;
  for (size_t arg_index = 0; arg_index < arguments.size(); ++arg_index) {
    threads.emplace_back(
        std::thread([&arguments, arg_index, &f] { f(arguments[arg_index]); }));

    if (threads.size() % batch_size == 0 || arg_index == arguments.size() - 1) {
      for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].join();
      }

      threads.clear();
    }
  }
}

}  // namespace common

#endif
