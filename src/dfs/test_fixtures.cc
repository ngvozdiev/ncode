#include "test_fixtures.h"
#include "../net/net_gen.h"

namespace ncode {
namespace dfs {
namespace test {

using namespace std::chrono;
static constexpr uint64_t kBwValue = 10000;

void SingleEdgeFixture::SetUp() {
  net::PBGraphLink* edge = graph_.add_links();
  edge->set_src(kNodeA);
  edge->set_dst(kNodeB);
  edge->set_src_port(10);
  edge->set_dst_port(11);
  edge->set_delay_sec(0.001);

  array_graph_ = ArrayGraph::NewArrayGraph(graph_, kNodeB, &storage_);
  ASSERT_NE(nullptr, array_graph_);

  vertex_id_to_offset_ = &array_graph_->vertex_id_to_offset();
  offset_to_vertex_id_ = &array_graph_->offset_to_vertex_id();
}

void BraessFixture::SetUp() {
  net::AddEdgeToGraph("C", "D", milliseconds(10), kBwValue, &graph_);
  net::AddEdgeToGraph("D", "C", milliseconds(10), kBwValue, &graph_);
  net::AddEdgeToGraph("B", "D", milliseconds(8), kBwValue, &graph_);
  net::AddEdgeToGraph("D", "B", milliseconds(8), kBwValue, &graph_);
  net::AddEdgeToGraph("A", "B", milliseconds(10), kBwValue, &graph_);
  net::AddEdgeToGraph("B", "A", milliseconds(10), kBwValue, &graph_);
  net::AddEdgeToGraph("A", "C", milliseconds(5), kBwValue, &graph_);
  net::AddEdgeToGraph("C", "A", milliseconds(5), kBwValue, &graph_);
  net::AddEdgeToGraph("B", "C", milliseconds(1), kBwValue, &graph_);

  array_graph_ = ArrayGraph::NewArrayGraph(graph_, dst_, &storage_);
  ASSERT_NE(nullptr, array_graph_);

  vertex_id_to_offset_ = &array_graph_->vertex_id_to_offset();
  offset_to_vertex_id_ = &array_graph_->offset_to_vertex_id();
}

CliqueFixture::CliqueFixture(uint32_t node_count) {
  graph_ = net::GenerateFullGraph(node_count, 1000, milliseconds(1));
}

void CliqueFixture::SetUp() {
  array_graph_ = ArrayGraph::NewArrayGraph(graph_, "N0", &storage_);
  ASSERT_NE(nullptr, array_graph_);
}

}  // namepsace test
}  // namespace dfs
}  // namespace ncode
