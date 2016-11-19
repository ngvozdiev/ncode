#include "algorithm.h"

#include <cstdint>
#include <limits>
#include <vector>
#include <thread>

#include "../common/substitute.h"
#include "net_gen.h"
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

TEST(Cluster, SingleLink) {
  PBNet net;
  AddEdgeToGraph("A", "B", Delay(100), kBw, &net);

  GraphStorage graph_storage(net);
  SimpleDirectedGraph graph(&graph_storage);
  DistanceClusteredGraph clustered_graph({}, Delay(0), &graph);

  // There should be 2 clusters.
  const GraphNodeMap<DistanceClusterIndex>& node_to_cluster =
      clustered_graph.node_to_cluster();
  ASSERT_EQ(2ul, node_to_cluster.Count());
  ASSERT_EQ(2ul, clustered_graph.AllClusters().Count());

  DistanceClusterIndex a_cluster =
      clustered_graph.GetClusterForNode(graph_storage.NodeFromStringOrDie("A"));
  DistanceClusterIndex b_cluster =
      clustered_graph.GetClusterForNode(graph_storage.NodeFromStringOrDie("B"));
  ASSERT_NE(a_cluster, b_cluster);
}

TEST(Cluster, SingleLinkSingleCluster) {
  PBNet net;
  AddEdgeToGraph("A", "B", Delay(100), kBw, &net);

  GraphStorage graph_storage(net);
  SimpleDirectedGraph graph(&graph_storage);
  DistanceClusteredGraph clustered_graph({}, Delay(100), &graph);

  // There should be 1 cluster.
  ASSERT_EQ(1ul, clustered_graph.AllClusters().Count());

  DistanceClusterIndex a_cluster =
      clustered_graph.GetClusterForNode(graph_storage.NodeFromStringOrDie("A"));
  DistanceClusterIndex b_cluster =
      clustered_graph.GetClusterForNode(graph_storage.NodeFromStringOrDie("B"));
  ASSERT_EQ(a_cluster, b_cluster);
}

TEST(Cluster, Braess) {
  PBNet net = GenerateBraess(kBw);
  GraphStorage graph_storage(net);
  SimpleDirectedGraph graph(&graph_storage);
  DistanceClusteredGraph c_zero({}, Delay(0), &graph);
  ASSERT_EQ(4ul, c_zero.AllClusters().Count());

  DistanceClusteredGraph c_one({}, Delay(1), &graph);
  ASSERT_EQ(4ul, c_one.AllClusters().Count());
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
  KShortestPaths ksp({}, {}, node_a, node_b, &graph);

  Links model;
  ASSERT_EQ(model, all_pair_sp.GetPath(node_b, node_a).links());
  ASSERT_EQ(model, all_pair_sp.GetPath(node_b, node_b).links());
  ASSERT_EQ(model, all_pair_sp.GetPath(node_a, node_a).links());
  ASSERT_EQ(model, sp.GetPath(node_a).links());

  model.emplace_back(link);
  ASSERT_EQ(model, all_pair_sp.GetPath(node_a, node_b).links());
  ASSERT_EQ(model, sp.GetPath(node_b).links());

  ASSERT_EQ(model, ksp.NextPath().links());
  ASSERT_TRUE(ksp.NextPath().empty());
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
  KShortestPaths ksp({}, {}, node_a, node_c, &graph);

  Links model;
  ASSERT_EQ(model, all_pair_sp.GetPath(node_b, node_a).links());

  model = {link_ab, link_bc};
  ASSERT_EQ(model, all_pair_sp.GetPath(node_a, node_c).links());
  ASSERT_EQ(model, sp.GetPath(node_c).links());
  ASSERT_EQ(model, ksp.NextPath().links());
  ASSERT_TRUE(ksp.NextPath().empty());

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
  KShortestPaths ksp({}, {}, node_a, node_d, &graph);

  Links model = {link_ad};
  ASSERT_EQ(model, all_pair_sp.GetPath(node_a, node_d).links());
  ASSERT_EQ(model, sp.GetPath(node_d).links());
  ASSERT_EQ(model, ksp.NextPath().links());

  GraphLinkIndex link_ab = graph_storage.LinkOrDie("A", "B");
  GraphLinkIndex link_bc = graph_storage.LinkOrDie("B", "C");
  GraphLinkIndex link_cd = graph_storage.LinkOrDie("C", "D");

  model = {link_ab, link_bc, link_cd};
  ASSERT_EQ(model, ksp.NextPath().links());
  ASSERT_TRUE(ksp.NextPath().empty());
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

  GraphLinkSet links_to_exclude = {link_ab};
  GraphSearchAlgorithmConfig config;
  config.AddToExcludeLinks(&links_to_exclude);

  SimpleDirectedGraph graph(&graph_storage);
  AllPairShortestPath all_pair_sp(config, &graph);
  ShortestPath sp(config, node_a, &graph);

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
  using namespace std::chrono;
  PBNet net = GenerateBraess(kBw);

  GraphStorage graph_storage(net);
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

TEST(KShortest, Braess) {
  PBNet net = GenerateBraess(kBw);

  GraphStorage graph_storage(net);
  GraphNodeIndex node_a = graph_storage.NodeFromStringOrDie("A");
  GraphNodeIndex node_d = graph_storage.NodeFromStringOrDie("D");
  GraphLinkIndex link_ab = graph_storage.LinkOrDie("A", "B");
  GraphLinkIndex link_ac = graph_storage.LinkOrDie("A", "C");
  GraphLinkIndex link_bc = graph_storage.LinkOrDie("B", "C");
  GraphLinkIndex link_cd = graph_storage.LinkOrDie("C", "D");
  GraphLinkIndex link_bd = graph_storage.LinkOrDie("B", "D");

  SimpleDirectedGraph graph(&graph_storage);
  KShortestPaths ksp({}, {}, node_a, node_d, &graph);

  std::vector<Links> model_paths = {
      {link_ac, link_cd}, {link_ab, link_bd}, {link_ab, link_bc, link_cd}};

  std::vector<Links> paths = {ksp.NextPath().links(), ksp.NextPath().links(),
                              ksp.NextPath().links()};
  ASSERT_EQ(model_paths, paths);
  ASSERT_TRUE(ksp.NextPath().empty());
}

}  // namespace
}  // namespace net
}  // namespace ncode
