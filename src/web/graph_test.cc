#include "graph.h"

#include <google/protobuf/repeated_field.h>

#include "../common/file.h"
#include "../net/net_common.h"
#include "../net/net_gen.h"
#include "gtest/gtest.h"
#include "net.pb.h"
#include "web_page.h"

namespace ncode {
namespace web {
namespace {

TEST(Graph, SimpleGraph) {
  net::PBNet net_pb =
      net::GenerateFullGraph(2, 10000, std::chrono::milliseconds(100));
  net::PathStorage path_storage;

  std::vector<EdgeData> edge_data;
  std::vector<PathData> path_data;
  std::vector<DisplayMode> display_modes;

  for (const auto& link_pb : net_pb.links()) {
    const net::GraphLink* link = path_storage.LinkFromProtobuf(link_pb);
    std::vector<double> values = {0.1, 0.9};
    edge_data.emplace_back(link, values, "Some tooltip", 0);
  }

  const net::GraphPath* path =
      path_storage.PathFromString("[N0->N1]", net_pb, 1);
  path_data.emplace_back(path, "Label 1", "Label 2");

  display_modes.emplace_back("Mode 1");
  display_modes.emplace_back("Mode 2");

  HtmlPage page;
  GraphToHTML(edge_data, path_data, display_modes, &page);
  File::WriteStringToFile(page.Construct(), "graph.html");
}

}  // namespace
}  // namespace web
}  // namespace ncode