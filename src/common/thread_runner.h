#ifndef NCODE_COMMON_THREAD_RUNNER_H
#define NCODE_COMMON_THREAD_RUNNER_H

#include <stddef.h>
#include <functional>
#include <thread>
#include <vector>
#include <mutex>

namespace ncode {

// Runs instances of a given function in parallel. At any given moment in time
// up to 'batch_size' function will run in parallel. This function will block
// and return when all functions have completed.
template <typename T>
void RunInParallel(const std::vector<T>& arguments,
                   std::function<void(const T&, size_t)> f, size_t batch = 4) {
  CHECK(batch > 0) << "Zero batch size";

  std::mutex mu;
  std::vector<bool> done(arguments.size(), false);

  std::vector<std::thread> threads;
  for (size_t j = 0; j < batch; ++j) {
    threads.emplace_back([&arguments, &f, &mu, &done] {
      mu.lock();
      for (size_t i = 0; i < arguments.size(); ++i) {
        if (!done[i]) {
          done[i] = true;
          mu.unlock();
          f(arguments[i], i);
          mu.lock();
        }
      }
      mu.unlock();
    });
  }

  for (size_t i = 0; i < batch; ++i) {
    threads[i].join();
  }
}

// Runs and maintains a number of instances of ProcessorBase.
template <typename Input, typename Result, typename Processor,
          size_t QueueSize = 1000>
class ThreadPool {
 public:
  using Factory = std::function<Processor()>;
  ThreadPool(size_t processor_count, Factory factory) {
    for (size_t i = 0; i < processor_count; ++i) {
      processors_.emplace_back(factory());
    }
  }

 private:
  using IdAndInput = std::pair<uint64_t, std::unique_ptr<Input>>;

  struct ProcessorState {
    ProcessorState(Processor processor, PtrQueue<Input, QueueSize>* input_queue)
        : processor(processor) {
      processor_thread = [this, input_queue] {
        while (true) {
          std::unique_ptr<Input> input = input_queue->ConsumeOrBlock();
          if (!input) {
            break;
          }

          processor.Process(input);
        }
      };
    }

    Processor processor;
    std::thread<void()> processor_thread;
  };

  // Work is added to this queue and processed by the processors.
  PtrQueue<IdAndInput, QueueSize> input_queue_;

  // The processors.
  std::vector<Processor> processors_;

  // Protects input_queue_.
  std::mutex mu_;
};

}  // namespace common

#endif
