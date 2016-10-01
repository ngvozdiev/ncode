#ifndef DFS_TEST_FIXTURES_H
#define DFS_TEST_FIXTURES_H

#include <chrono>
#include "array_graph.h"
#include "gtest/gtest.h"

namespace ncode {
namespace dfs {
namespace test {

const char kNodeA[] = "A";
const char kNodeB[] = "B";
const char kNodeC[] = "C";
const char kNodeD[] = "D";

class Timer {
 public:
  Timer() : start_point_(std::chrono::system_clock::now()) {}

  std::chrono::milliseconds DurationMillis() {
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now() - start_point_);
    return duration;
  }

 private:
  std::chrono::system_clock::time_point start_point_;
};

// A fixture that sets up a graph consisting of two nodes A and B and a single
// (unidirectional) link A -> B. Also the destination is set to B.
class SingleEdgeFixture : public ::testing::Test {
 protected:
  void SetUp() override;

  const std::map<std::string, ArrayGraphOffset>* vertex_id_to_offset_;
  const std::map<ArrayGraphOffset, std::string>* offset_to_vertex_id_;
  net::PathStorage storage_;
  net::PBNet graph_;
  std::unique_ptr<ArrayGraph> array_graph_;
};

// A fixture that sets up a simple example of Braess's paradox. It is a
// diamond-shaped graph with a "shortcut" in the middle.
class BraessFixture : public ::testing::Test {
 protected:
  BraessFixture(const std::string& dst)
      : dst_(dst),
        vertex_id_to_offset_(nullptr),
        offset_to_vertex_id_(nullptr) {}

  net::LinkSequence GetPath(const string& path_string) {
    return storage_.PathFromString(path_string, graph_, 0)->link_sequence();
  }

  void SetUp() override;

  const std::string dst_;
  const std::map<std::string, ArrayGraphOffset>* vertex_id_to_offset_;
  const std::map<ArrayGraphOffset, std::string>* offset_to_vertex_id_;
  net::PathStorage storage_;
  net::PBNet graph_;
  std::unique_ptr<ArrayGraph> array_graph_;
};

class BraessDstD : public BraessFixture {
 protected:
  BraessDstD() : BraessFixture(kNodeD) {}
};

class BraessDstC : public BraessFixture {
 protected:
  BraessDstC() : BraessFixture(kNodeC) {}
};

// A fixture that sets up a full graph. Destination is set to N0.
class CliqueFixture : public ::testing::Test {
 protected:
  CliqueFixture(uint32_t node_count);

  void SetUp() override;

  net::PBNet graph_;
  net::PathStorage storage_;
  std::unique_ptr<ArrayGraph> array_graph_;
};

// Should be big enough to cause long-running computation on any machine
// (complexity is factorial since we are looking for edge-disjoint paths).
class LongRunningFixture : public CliqueFixture {
 protected:
  LongRunningFixture() : CliqueFixture(10) {}
};

}  // namespace test
}  // namespace dfs
}  // namespace ncode

#endif /* PATHFINDER_TEST_FIXTURES_H */
