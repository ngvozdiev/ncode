#include "algorithm.h"

#include <cstdint>
#include <limits>
#include <vector>
#include <thread>

#include "../common/substitute.h"
#include "gtest/gtest.h"

namespace ncode {
namespace net {
namespace {

static constexpr Bandwidth kBw = Bandwidth::FromBitsPerSecond(100);

TEST(SimpleGraph, DoubleEdge) {
  PBNet net;
  AddEdgeToGraph("A", "B", Delay(100), kBw, &net);
  AddEdgeToGraph("A", "B", Delay(10), kBw, &net);

  GraphStorage graph_storage(net);
  ASSERT_DEATH(SimpleDirectedGraph graph(&graph_storage), "Double edge");
}

TEST(AllPairShortestPath, SingleLink) {
  PBNet net;
  AddEdgeToGraph("A", "B", Delay(100), kBw, &net);

  GraphStorage graph_storage(net);
  GraphNodeIndex node_a = graph_storage.NodeFromStringOrDie("A");
  GraphNodeIndex node_b = graph_storage.NodeFromStringOrDie("B");
  GraphLinkIndex link = graph_storage.LinkOrDie("A", "B");

  SimpleDirectedGraph graph(&graph_storage);
  AllPairShortestPath all_pair_sp({}, &graph);
  ShortestPath sp({}, node_a, &graph);

  Links model;
  ASSERT_EQ(model, all_pair_sp.GetPath(node_b, node_a).links());
  ASSERT_EQ(model, all_pair_sp.GetPath(node_b, node_b).links());
  ASSERT_EQ(model, all_pair_sp.GetPath(node_a, node_a).links());
  ASSERT_EQ(model, sp.GetPath(node_a).links());

  model.emplace_back(link);
  ASSERT_EQ(model, all_pair_sp.GetPath(node_a, node_b).links());
  ASSERT_EQ(model, sp.GetPath(node_b).links());
}

TEST(AllPairShortestPath, ShortPath) {
  PBNet net;
  AddEdgeToGraph("A", "B", Delay(100), kBw, &net);
  AddEdgeToGraph("B", "C", Delay(100), kBw, &net);
  AddEdgeToGraph("C", "B", Delay(100), kBw, &net);

  GraphStorage graph_storage(net);
  GraphNodeIndex node_a = graph_storage.NodeFromStringOrDie("A");
  GraphNodeIndex node_b = graph_storage.NodeFromStringOrDie("B");
  GraphNodeIndex node_c = graph_storage.NodeFromStringOrDie("C");
  GraphLinkIndex link_ab = graph_storage.LinkOrDie("A", "B");
  GraphLinkIndex link_bc = graph_storage.LinkOrDie("B", "C");
  GraphLinkIndex link_cb = graph_storage.LinkOrDie("C", "B");

  SimpleDirectedGraph graph(&graph_storage);
  AllPairShortestPath all_pair_sp({}, &graph);
  ShortestPath sp({}, node_a, &graph);

  Links model;
  ASSERT_EQ(model, all_pair_sp.GetPath(node_b, node_a).links());

  model = {link_ab, link_bc};
  ASSERT_EQ(model, all_pair_sp.GetPath(node_a, node_c).links());
  ASSERT_EQ(model, sp.GetPath(node_c).links());

  model = {link_cb};
  ASSERT_EQ(model, all_pair_sp.GetPath(node_c, node_b).links());
}

TEST(AllPairShortestPath, ShorterPath) {
  PBNet net;
  AddEdgeToGraph("A", "B", Delay(100), kBw, &net);
  AddEdgeToGraph("B", "C", Delay(100), kBw, &net);
  AddEdgeToGraph("C", "D", Delay(100), kBw, &net);
  AddEdgeToGraph("A", "D", Delay(100), kBw, &net);

  GraphStorage graph_storage(net);
  GraphNodeIndex node_a = graph_storage.NodeFromStringOrDie("A");
  GraphNodeIndex node_d = graph_storage.NodeFromStringOrDie("D");
  GraphLinkIndex link_ad = graph_storage.LinkOrDie("A", "D");

  SimpleDirectedGraph graph(&graph_storage);
  AllPairShortestPath all_pair_sp({}, &graph);
  ShortestPath sp({}, node_a, &graph);

  Links model = {link_ad};
  ASSERT_EQ(model, all_pair_sp.GetPath(node_a, node_d).links());
  ASSERT_EQ(model, sp.GetPath(node_d).links());
}

TEST(AllPairShortestPath, ShortPathMask) {
  PBNet net;
  AddEdgeToGraph("A", "B", Delay(100), kBw, &net);
  AddEdgeToGraph("B", "C", Delay(100), kBw, &net);

  GraphStorage graph_storage(net);
  GraphNodeIndex node_a = graph_storage.NodeFromStringOrDie("A");
  GraphNodeIndex node_b = graph_storage.NodeFromStringOrDie("B");
  GraphNodeIndex node_c = graph_storage.NodeFromStringOrDie("C");
  GraphLinkIndex link_ab = graph_storage.LinkOrDie("A", "B");
  GraphLinkIndex link_bc = graph_storage.LinkOrDie("B", "C");

  DeprefSearchAlgorithmArgs args;
  args.links_to_exclude = {link_ab};

  SimpleDirectedGraph graph(&graph_storage);
  AllPairShortestPath all_pair_sp(args, &graph);
  ShortestPath sp(args, node_a, &graph);

  Links model;
  ASSERT_EQ(model, all_pair_sp.GetPath(node_a, node_c).links());
  ASSERT_EQ(model, all_pair_sp.GetPath(node_a, node_b).links());
  ASSERT_EQ(model, sp.GetPath(node_b).links());
  ASSERT_EQ(model, sp.GetPath(node_c).links());

  model = {link_bc};
  ASSERT_EQ(model, all_pair_sp.GetPath(node_b, node_c).links());
}

TEST(DFS, SingleLink) {
  PBNet net;
  AddEdgeToGraph("A", "B", Delay(100), kBw, &net);

  GraphStorage graph_storage(net);
  GraphNodeIndex node_a = graph_storage.NodeFromStringOrDie("A");
  GraphNodeIndex node_b = graph_storage.NodeFromStringOrDie("B");
  GraphLinkIndex link_ab = graph_storage.LinkOrDie("A", "B");

  SimpleDirectedGraph graph(&graph_storage);
  DFS dfs({}, &graph);

  std::vector<Links> paths;
  dfs.Paths(node_a, node_b, Delay(100), 10, [&paths](const LinkSequence& path) {
    paths.emplace_back(path.links());
  });

  std::vector<Links> model_paths = {{link_ab}};
  ASSERT_EQ(model_paths, paths);

  paths.clear();
  dfs.Paths(node_a, node_b, Delay(99), 10, [&paths](const LinkSequence& path) {
    paths.emplace_back(path.links());
  });
  ASSERT_TRUE(paths.empty());
}

TEST(DFS, MultiPath) {
  PBNet net;
  AddEdgeToGraph("A", "B", Delay(100), kBw, &net);
  AddEdgeToGraph("B", "C", Delay(100), kBw, &net);
  AddEdgeToGraph("C", "D", Delay(100), kBw, &net);
  AddEdgeToGraph("A", "D", Delay(100), kBw, &net);

  GraphStorage graph_storage(net);
  GraphNodeIndex node_a = graph_storage.NodeFromStringOrDie("A");
  GraphNodeIndex node_d = graph_storage.NodeFromStringOrDie("D");
  GraphLinkIndex link_ab = graph_storage.LinkOrDie("A", "B");
  GraphLinkIndex link_bc = graph_storage.LinkOrDie("B", "C");
  GraphLinkIndex link_cd = graph_storage.LinkOrDie("C", "D");
  GraphLinkIndex link_ad = graph_storage.LinkOrDie("A", "D");

  SimpleDirectedGraph graph(&graph_storage);
  DFS dfs({}, &graph);

  std::vector<Links> paths;
  dfs.Paths(
      node_a, node_d, Delay(1000), 10,
      [&paths](const LinkSequence& path) { paths.emplace_back(path.links()); });

  std::vector<Links> model_paths = {{link_ab, link_bc, link_cd}, {link_ad}};
  ASSERT_EQ(model_paths, paths);
}

TEST(DFS, Braess) {
  PBNet net;

  using namespace std::chrono;
  AddEdgeToGraph("C", "D", milliseconds(10), kBw, &net);
  AddEdgeToGraph("D", "C", milliseconds(10), kBw, &net);
  AddEdgeToGraph("B", "D", milliseconds(8), kBw, &net);
  AddEdgeToGraph("D", "B", milliseconds(8), kBw, &net);
  AddEdgeToGraph("A", "B", milliseconds(10), kBw, &net);
  AddEdgeToGraph("B", "A", milliseconds(10), kBw, &net);
  AddEdgeToGraph("A", "C", milliseconds(5), kBw, &net);
  AddEdgeToGraph("C", "A", milliseconds(5), kBw, &net);
  AddEdgeToGraph("B", "C", milliseconds(1), kBw, &net);

  PathStorage graph_storage(net);
  GraphNodeIndex node_a = graph_storage.NodeFromStringOrDie("A");
  GraphNodeIndex node_d = graph_storage.NodeFromStringOrDie("D");

  SimpleDirectedGraph graph(&graph_storage);
  DFS dfs({}, &graph);

  std::vector<const GraphPath*> paths;
  dfs.Paths(node_a, node_d, duration_cast<Delay>(seconds(1)), 10,
            [&graph_storage, &paths](const LinkSequence& path) {
              paths.emplace_back(graph_storage.PathFromLinksOrDie(path, 0));
            });

  // The default metric depth limit should be enough to capture all three paths
  ASSERT_EQ(3ul, paths.size());
  ASSERT_TRUE(IsInPaths("[A->C, C->D]", paths, 0, &graph_storage));
  ASSERT_TRUE(IsInPaths("[A->B, B->D]", paths, 0, &graph_storage));
  ASSERT_TRUE(IsInPaths("[A->B, B->C, C->D]", paths, 0, &graph_storage));
}

}  // namespace
}  // namespace net
}  // namespace ncode
