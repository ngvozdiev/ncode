#include <chrono>

#include "net.pb.h"
#include "../common/common.h"
#include "../common/logging.h"
#include "algorithm.h"
#include "net_common.h"
#include "net_gen.h"

using namespace ncode;
using namespace std::chrono;

int main(int argc, char** argv) {
  Unused(argc);
  Unused(argv);

  net::PBNet net = net::GenerateNTT();
  net::PathStorage path_storage(net);

  net::SimpleDirectedGraph graph(&path_storage);

  auto now = high_resolution_clock::now();
  net::AllPairShortestPath all_pair_sp(&graph);
  auto later = high_resolution_clock::now();
  auto duration = duration_cast<milliseconds>(later - now);
  LOG(ERROR) << "All pair shortest paths in " << duration.count() << "ms";

  net::GraphNodeIndex london_node = path_storage.NodeFromStringOrDie("london");
  net::GraphNodeIndex osaka_node = path_storage.NodeFromStringOrDie("osaka");

  now = high_resolution_clock::now();
  for (size_t i = 0; i < 1000; ++i) {
    net::ShortestPath sp(london_node, &graph);
    sp.GetPath(osaka_node);
  }
  later = high_resolution_clock::now();
  duration = duration_cast<milliseconds>(later - now);
  LOG(ERROR) << "1000 shortest paths in " << duration.count() << "ms";

  net::DFS dfs(&graph);
  now = high_resolution_clock::now();
  std::vector<net::LinkSequence> paths;
  dfs.Paths(
      london_node, osaka_node, duration_cast<net::Delay>(seconds(1)), 10,
      [&paths](const net::LinkSequence& path) { paths.emplace_back(path); });
  later = high_resolution_clock::now();
  duration = duration_cast<milliseconds>(later - now);
  LOG(ERROR) << paths.size() << " paths in " << duration.count() << "ms";
}
