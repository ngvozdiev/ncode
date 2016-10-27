#include <limits.h>

#include "gtest/gtest.h"
#include "array_graph.h"
#include "test_fixtures.h"

namespace ncode {
namespace dfs {
namespace test {

TEST(InitTest, BadGraphBadDest) {
  net::PathStorage storage;
  net::PBNet graph;

  net::PBGraphLink* edge = graph.add_links();
  edge->set_src(kNodeA);
  edge->set_dst(kNodeB);
  edge->set_src_port(10);
  edge->set_dst_port(11);
  edge->set_delay_sec(0.010);

  ASSERT_DEATH(ArrayGraph::NewArrayGraph(graph, kNodeC, &storage), ".*");
}

TEST(InitTest, BadEdgeNoDest) {
  net::PathStorage storage;
  net::PBNet graph;

  net::PBGraphLink* edge = graph.add_links();
  edge->set_src(kNodeA);
  edge->set_src_port(10);
  edge->set_dst_port(11);
  edge->set_delay_sec(0.010);

  ASSERT_DEATH(ArrayGraph::NewArrayGraph(graph, kNodeA, &storage), ".*");
}

TEST(InitTest, BadEdgeNoSrc) {
  net::PathStorage storage;
  net::PBNet graph;

  net::PBGraphLink* edge = graph.add_links();
  edge->set_dst(kNodeA);
  edge->set_src_port(10);
  edge->set_dst_port(11);
  edge->set_delay_sec(0.010);

  ASSERT_DEATH(ArrayGraph::NewArrayGraph(graph, kNodeA, &storage), ".*");
}

TEST(InitTest, BadEdgeNoSport) {
  net::PathStorage storage;
  net::PBNet graph;

  net::PBGraphLink* edge = graph.add_links();
  edge->set_src(kNodeA);
  edge->set_dst(kNodeB);
  edge->set_dst_port(11);
  edge->set_delay_sec(0.010);

  ASSERT_DEATH(ArrayGraph::NewArrayGraph(graph, kNodeA, &storage), ".*");
}

TEST(InitTest, BadEdgeNoDport) {
  net::PathStorage storage;
  net::PBNet graph;

  net::PBGraphLink* edge = graph.add_links();
  edge->set_src(kNodeA);
  edge->set_dst(kNodeB);
  edge->set_src_port(11);
  edge->set_delay_sec(0.010);

  ASSERT_DEATH(ArrayGraph::NewArrayGraph(graph, kNodeA, &storage), ".*");
}

TEST(InitTest, BadEdgeSrcSameAsDest) {
  net::PathStorage storage;
  net::PBNet graph;

  net::PBGraphLink* edge = graph.add_links();
  edge->set_src(kNodeA);
  edge->set_dst(kNodeA);
  edge->set_src_port(10);
  edge->set_dst_port(11);
  edge->set_delay_sec(0.010);

  ASSERT_DEATH(ArrayGraph::NewArrayGraph(graph, kNodeA, &storage), ".*");
}

TEST(InitTest, BadDoubleEdgeSameSrcPorts) {
  net::PathStorage storage;
  net::PBNet graph;

  net::PBGraphLink* edge = graph.add_links();
  edge->set_src(kNodeA);
  edge->set_dst(kNodeB);
  edge->set_src_port(10);
  edge->set_dst_port(11);
  edge->set_delay_sec(0.002);

  edge = graph.add_links();
  edge->set_src(kNodeA);
  edge->set_dst(kNodeB);
  edge->set_src_port(10);
  edge->set_dst_port(15);
  edge->set_delay_sec(0.003);

  ASSERT_DEATH(ArrayGraph::NewArrayGraph(graph, kNodeB, &storage), ".*");
}

TEST(InitTest, BadDoubleEdgeSameDstPorts) {
  net::PathStorage storage;
  net::PBNet graph;

  net::PBGraphLink* edge = graph.add_links();
  edge->set_src(kNodeA);
  edge->set_dst(kNodeB);
  edge->set_src_port(10);
  edge->set_dst_port(11);
  edge->set_delay_sec(0.002);

  edge = graph.add_links();
  edge->set_src(kNodeA);
  edge->set_dst(kNodeB);
  edge->set_src_port(11);
  edge->set_dst_port(11);
  edge->set_delay_sec(0.003);

  ASSERT_DEATH(ArrayGraph::NewArrayGraph(graph, kNodeB, &storage), ".*");
}

TEST(InitTest, DoubleEdge) {
  net::PathStorage storage;
  net::PBNet graph;

  net::PBGraphLink* edge = graph.add_links();
  edge->set_src(kNodeA);
  edge->set_dst(kNodeB);
  edge->set_src_port(10);
  edge->set_dst_port(11);
  edge->set_delay_sec(0.002);

  edge = graph.add_links();
  edge->set_src(kNodeA);
  edge->set_dst(kNodeB);
  edge->set_src_port(12);
  edge->set_dst_port(13);
  edge->set_delay_sec(0.003);

  auto agraph = ArrayGraph::NewArrayGraph(graph, kNodeB, &storage);

  ASSERT_EQ(2ul, agraph->vertex_id_to_offset().size());
  ASSERT_EQ(2ul, agraph->offset_to_vertex_id().size());

  ASSERT_EQ(1ul, agraph->vertex_id_to_offset().count(kNodeA));
  ASSERT_EQ(1ul, agraph->vertex_id_to_offset().count(kNodeB));

  ArrayGraphOffset offset_a =
      agraph->vertex_id_to_offset().find(kNodeA)->second;
  ArrayGraphOffset offset_b =
      agraph->vertex_id_to_offset().find(kNodeB)->second;

  // The graph contains two unidirectional edges
  ASSERT_EQ(2, agraph->GetNeighborCount(offset_a));
  ASSERT_EQ(0, agraph->GetNeighborCount(offset_b));
  ASSERT_EQ(offset_b, agraph->GetOffsetOfNeighbor(offset_a, 0));
  ASSERT_EQ(offset_b, agraph->GetOffsetOfNeighbor(offset_a, 1));
  ASSERT_EQ(2000, agraph->GetDistanceToNeighbor(offset_a, 0));
  ASSERT_EQ(3000, agraph->GetDistanceToNeighbor(offset_a, 1));

  // The distance to B should be 2 - the shorter edge
  ASSERT_EQ(2000, agraph->GetDistanceToDest(offset_a));
}

TEST_F(SingleEdgeFixture, OneEdge) {
  ASSERT_EQ(2ul, vertex_id_to_offset_->size());
  ASSERT_EQ(2ul, offset_to_vertex_id_->size());

  ASSERT_EQ(1ul, vertex_id_to_offset_->count(kNodeA));
  ASSERT_EQ(1ul, vertex_id_to_offset_->count(kNodeB));

  ArrayGraphOffset offset_a = vertex_id_to_offset_->find(kNodeA)->second;
  ArrayGraphOffset offset_b = vertex_id_to_offset_->find(kNodeB)->second;

  ASSERT_EQ(kNodeA, offset_to_vertex_id_->find(offset_a)->second);
  ASSERT_EQ(kNodeB, offset_to_vertex_id_->find(offset_b)->second);

  // The graph contains only one unidirectional edge
  ASSERT_EQ(1, array_graph_->GetNeighborCount(offset_a));
  ASSERT_EQ(0, array_graph_->GetNeighborCount(offset_b));
  ASSERT_EQ(offset_b, array_graph_->GetOffsetOfNeighbor(offset_a, 0));
}

TEST_F(SingleEdgeFixture, OneEdgeDefaultWeight) {
  ArrayGraphOffset offset_a = vertex_id_to_offset_->find(kNodeA)->second;
  ArrayGraphOffset offset_b = vertex_id_to_offset_->find(kNodeB)->second;

  // The default weight if not specified should be 1000
  ASSERT_EQ(1000, array_graph_->GetDistanceToNeighbor(offset_a, 0));

  // The distance from A to the destination should be 1 (B is the destination)
  ASSERT_EQ(1000, array_graph_->GetDistanceToDest(offset_a));

  // The distance from B to the destination should be zero
  ASSERT_EQ(0, array_graph_->GetDistanceToDest(offset_b));
}

TEST_F(SingleEdgeFixture, Marking) {
  ArrayGraphOffset offset_a = vertex_id_to_offset_->find(kNodeA)->second;
  ArrayGraphOffset offset_b = vertex_id_to_offset_->find(kNodeB)->second;

  // Initially no node should be marked
  ASSERT_FALSE(array_graph_->IsVertexMarked(offset_a));
  ASSERT_FALSE(array_graph_->IsVertexMarked(offset_b));

  // Mark only one vertex
  array_graph_->MarkVertex(offset_a);
  ASSERT_TRUE(array_graph_->IsVertexMarked(offset_a));
  ASSERT_FALSE(array_graph_->IsVertexMarked(offset_b));

  // Mark the other one
  array_graph_->MarkVertex(offset_b);
  ASSERT_TRUE(array_graph_->IsVertexMarked(offset_a));
  ASSERT_TRUE(array_graph_->IsVertexMarked(offset_b));

  // Unmark all
  array_graph_->UnmarkAllVertices();
  ASSERT_FALSE(array_graph_->IsVertexMarked(offset_a));
  ASSERT_FALSE(array_graph_->IsVertexMarked(offset_b));
}

TEST_F(BraessDstD, Distances) {
  ArrayGraphOffset offset_a = vertex_id_to_offset_->find(kNodeA)->second;
  ArrayGraphOffset offset_b = vertex_id_to_offset_->find(kNodeB)->second;
  ArrayGraphOffset offset_c = vertex_id_to_offset_->find(kNodeC)->second;
  ArrayGraphOffset offset_d = vertex_id_to_offset_->find(kNodeD)->second;

  ASSERT_EQ(15000, array_graph_->GetDistanceToDest(offset_a));
  ASSERT_EQ(8000, array_graph_->GetDistanceToDest(offset_b));
  ASSERT_EQ(10000, array_graph_->GetDistanceToDest(offset_c));
  ASSERT_EQ(0, array_graph_->GetDistanceToDest(offset_d));
}

TEST_F(BraessDstC, Distances) {
  ArrayGraphOffset offset_a = vertex_id_to_offset_->find(kNodeA)->second;
  ArrayGraphOffset offset_b = vertex_id_to_offset_->find(kNodeB)->second;
  ArrayGraphOffset offset_c = vertex_id_to_offset_->find(kNodeC)->second;
  ArrayGraphOffset offset_d = vertex_id_to_offset_->find(kNodeD)->second;

  ASSERT_EQ(5000, array_graph_->GetDistanceToDest(offset_a));
  ASSERT_EQ(1000, array_graph_->GetDistanceToDest(offset_b));
  ASSERT_EQ(0, array_graph_->GetDistanceToDest(offset_c));
  ASSERT_EQ(9000, array_graph_->GetDistanceToDest(offset_d));
}

TEST_F(BraessDstC, TestPathFromProtobufEmpty) {
  std::vector<net::PBGraphLink> path;

  // Empty path
  auto result = storage_.PathFromProtobuf(path, 0);
  ASSERT_TRUE(result->empty());
}

TEST_F(BraessDstC, TestPathFromProtobufBadEdgeOne) {
  std::vector<net::PBGraphLink> path;
  net::PBGraphLink edge;
  edge.set_src("A");
  edge.set_src("Z");
  path.push_back(edge);

  // Non-existent edge
  ASSERT_DEATH(storage_.PathFromProtobuf(path, 0), ".*");
}

TEST_F(BraessDstC, TestPathFromProtobufSingleEdge) {
  std::vector<net::PBGraphLink> path;
  net::PBGraphLink edge;
  edge.set_src("A");
  edge.set_dst("B");
  path.push_back(edge);

  // Path of one edge
  auto ag_path = storage_.PathFromProtobuf(path, 0);
  ASSERT_EQ(1ul, ag_path->size());
  ASSERT_EQ("[A:5->B:5]", ag_path->ToString());
}

TEST_F(BraessDstC, TestPathDifferentAggregates) {
  std::vector<net::PBGraphLink> path;
  net::PBGraphLink edge;
  edge.set_src("A");
  edge.set_dst("B");
  path.push_back(edge);

  const net::GraphPath* ag_path = storage_.PathFromProtobuf(path, 0);
  const net::GraphPath* other_path = storage_.PathFromProtobuf(path, 0);
  const net::GraphPath* path_diff_aggregate =
      storage_.PathFromProtobuf(path, 1);

  ASSERT_EQ(ag_path, other_path);
  ASSERT_NE(ag_path, path_diff_aggregate);
}

TEST_F(BraessDstC, TestPathFromProtobufMultiEdge) {
  std::vector<net::PBGraphLink> path;
  net::PBGraphLink edge_one;
  edge_one.set_src("A");
  edge_one.set_dst("B");
  path.push_back(edge_one);

  net::PBGraphLink edge_two;
  edge_two.set_src("B");
  edge_two.set_dst("D");
  path.push_back(edge_two);

  auto ag_path = storage_.PathFromProtobuf(path, 0);

  ASSERT_EQ(2ul, ag_path->size());
  ASSERT_EQ("[A:5->B:5, B:3->D:3]", ag_path->ToString());
}

TEST_F(BraessDstC, TestPathFromProtobufNonContiguous) {
  std::vector<net::PBGraphLink> path;
  net::PBGraphLink edge_one;
  edge_one.set_src("A");
  edge_one.set_dst("B");
  path.push_back(edge_one);

  net::PBGraphLink edge_two;
  edge_two.set_src("D");
  edge_two.set_dst("B");
  path.push_back(edge_two);

  ASSERT_DEATH(storage_.PathFromProtobuf(path, 0), ".*");
}

TEST(ToTreeTest, EmptyGraph) {
  net::PathStorage storage;
  net::PBNet graph;

  ASSERT_DEATH(ToTree(graph, "A", &storage), ".*");
}

TEST(ToTreeTest, BadRoot) {
  net::PathStorage storage;
  net::PBNet graph;

  net::PBGraphLink* edge = graph.add_links();
  edge->set_src(kNodeA);
  edge->set_dst(kNodeB);
  edge->set_src_port(10);
  edge->set_dst_port(10);
  edge->set_delay_sec(0.001);

  ASSERT_DEATH(ToTree(graph, "D", &storage), ".*");
}

TEST(ToTreeTest, SingleEdge) {
  net::PathStorage storage;
  net::PBNet graph;

  net::PBGraphLink* edge = graph.add_links();
  edge->set_src(kNodeA);
  edge->set_dst(kNodeB);
  edge->set_src_port(10);
  edge->set_dst_port(10);
  edge->set_delay_sec(0.001);

  auto tree = ToTree(graph, "A", &storage);

  ASSERT_EQ(1, tree.links_size());

  // The new graph should have a copy of the edge.
  ASSERT_NE(&tree.links(0), edge);
  ASSERT_EQ(tree.links(0).DebugString(), edge->DebugString());
}

TEST(ToTreeTest, DoubleEdge) {
  net::PathStorage storage;
  net::PBNet graph;

  net::PBGraphLink* edge = graph.add_links();
  edge->set_src(kNodeA);
  edge->set_dst(kNodeB);
  edge->set_src_port(10);
  edge->set_dst_port(10);
  edge->set_delay_sec(0.001);

  edge = graph.add_links();
  edge->set_src(kNodeA);
  edge->set_dst(kNodeB);
  edge->set_src_port(11);
  edge->set_dst_port(11);
  edge->set_delay_sec(0.001);

  auto tree = ToTree(graph, "A", &storage);

  // There are two edges from A to B. The tree should only contain one of them
  // (does not matter which one).
  ASSERT_EQ(1, tree.links_size());
}

TEST(ToTreeTest, BidirectionalEdge) {
  net::PathStorage storage;
  net::PBNet graph;

  net::PBGraphLink* edge = graph.add_links();
  edge->set_src(kNodeA);
  edge->set_dst(kNodeB);
  edge->set_src_port(10);
  edge->set_dst_port(10);
  edge->set_delay_sec(0.001);

  edge = graph.add_links();
  edge->set_src(kNodeB);
  edge->set_dst(kNodeA);
  edge->set_src_port(10);
  edge->set_dst_port(10);
  edge->set_delay_sec(0.001);

  auto tree = ToTree(graph, "A", &storage);

  // The tree should contain only one edge (A->B)
  ASSERT_EQ(1, tree.links_size());
}

TEST_F(BraessDstC, ToTree) {
  auto tree = ToTree(graph_, "C", &storage_);

  // The tree should have n - 1 edges (n is the number of vertices) and each
  // vertex should be the destination of exactly one edge, except for the root
  // which should only be a source
  int num_vertices = array_graph_->vertex_id_to_offset().size();
  ASSERT_EQ(tree.links_size(), num_vertices - 1);

  std::set<std::string> destinations;
  for (const auto& edge : tree.links()) {
    ASSERT_FALSE(destinations.count(edge.dst()));
    destinations.insert(edge.dst());
  }

  ASSERT_FALSE(destinations.count("C"));
}

}  // namespace test
}  // namespace dfs
}  // namespace ncode
