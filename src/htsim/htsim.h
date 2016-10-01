#ifndef NCODE_HTSIM_HTSIM_H
#define NCODE_HTSIM_HTSIM_H

#include <chrono>
#include <cstdint>
#include <ctime>
#include <iostream>
#include <string>
#include <inttypes.h>

#include "../common/event_queue.h"
#include "../metrics/metrics.h"

namespace ncode {
namespace htsim {

// A generic component in the simulation. All objects that need access to the
// event queue will inherit from this class. Those that need to process events
// will also inherit from common::EventConsumer.
class SimComponent {
 public:
  // The id of this component.
  const std::string& id() { return id_; }

 protected:
  SimComponent(const std::string& id, EventQueue* event_queue)
      : id_(id), event_queue_(event_queue) {}

  // Uniquely identifies each component in the simulation.
  const std::string id_;

  // Each component has a non-owning pointer to the event queue.
  EventQueue* const event_queue_;
};

// A timestamp provider that is based off an event queue.
class SimTimestampProvider : public ncode::metrics::TimestampProviderInterface {
 public:
  static constexpr const char* kPicoseconds = "picoseconds";
  SimTimestampProvider(SimTimeEventQueue* event_queue)
      : event_queue_(event_queue) {}

  uint64_t GetTimestamp() const override {
    return event_queue_->CurrentTime().Raw();
  }

  const char* TimestampUnits() const override { return kPicoseconds; }

  std::string TimestampToString(uint64_t timestamp) const override {
    using std::chrono::system_clock;
    const auto dt = SimTimeEventQueue::picoseconds(timestamp);
    const std::chrono::time_point<system_clock> tp_after_duration(
        std::chrono::duration_cast<std::chrono::milliseconds>(dt));
    time_t time_after_duration = system_clock::to_time_t(tp_after_duration);
    uint64_t milliseconds_remainder = (timestamp / 1000) % 1000;

    char s[128];
    strftime(s, 128, "%H:%M:%S ", std::localtime(&time_after_duration));

    std::string time_str(s);
    time_str += std::to_string(milliseconds_remainder) + "ms";
    return time_str;
  }

 private:
  SimTimeEventQueue* event_queue_;
};

// class ProgressIndicator : public NoEventComponent {
// public:
//  ProgressIndicator(Component* parent, const std::string& component_id,
//                    EventQueueTime end_time);
//
// private:
//  // Displays the current progress to stdout and updates the metric.
//  double Update();
//
//  // The time the simulation ends.
//  EventQueueTime end_time_;
//
//  // The initial time.
//  std::chrono::milliseconds init_real_time_;
//};

}  // namespace htsim
}  // namespace ncode

#endif
