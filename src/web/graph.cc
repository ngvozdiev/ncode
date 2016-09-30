#include "graph.h"

#include <ctemplate/template.h>
#include <ctemplate/template_dictionary.h>
#include <ctemplate/template_enums.h>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include "../common/logging.h"
#include "../net/net_common.h"
#include "json.hpp"
#include "web_page.h"

namespace ncode {
namespace web {

extern "C" const unsigned char www_graph_html[];
extern "C" const unsigned www_graph_html_size;
extern "C" const unsigned char www_graph_style_html[];
extern "C" const unsigned www_graph_style_html_size;

static constexpr char kGraphKey[] = "graph";
static constexpr char kGraphJSONKey[] = "graph_json";
static constexpr char kPathJSONKey[] = "paths_json";
static constexpr char kDisplayModeSectionMarker[] = "display_mode_section";
static constexpr char kDisplayModeKey[] = "display_mode";

// Link data in the format that the HTML template expects. The data will be
// converted to json later.
struct LinkDataHelper {
  size_t src_index;
  size_t dst_index;
  std::vector<double> forward_load;
  std::vector<double> reverse_load;
  std::string forward_tooltip;
  std::string reverse_tooltip;
  size_t distance_hint;
};

// Like LinkData, but for paths.
struct PathDataHelper {
  std::vector<size_t> node_indices;
  std::string label;
  std::string legend_label;
};

static size_t GetNodeIndex(const std::string& node_id,
                           std::map<std::string, size_t>* node_index_map) {
  auto it = node_index_map->find(node_id);
  if (it != node_index_map->end()) {
    return it->second;
  }

  size_t return_index = node_index_map->size();
  (*node_index_map)[node_id] = return_index;
  return return_index;
}

using LinkDataMap = std::map<size_t, std::map<size_t, LinkDataHelper>>;

void GraphToHTML(const std::vector<EdgeData>& edges,
                 const std::vector<PathData>& paths,
                 const std::vector<DisplayMode>& display_modes, HtmlPage* out,
                 LocalizerCallback localizer) {
  CHECK(!display_modes.empty()) << "At least one display mode required";

  // Mapping from the node id to the sequential index of the node.
  std::map<std::string, size_t> node_id_to_node_index;
  std::set<const LinkDataHelper*> link_data_set;

  // All links are bidirectional.
  LinkDataMap src_to_dst_to_link_data;

  // The paths are not required to be in any particular order.
  std::vector<PathDataHelper> all_paths;

  for (const EdgeData& edge_data : edges) {
    const std::string& src = edge_data.link->src();
    const std::string& dst = edge_data.link->dst();

    CHECK(src != dst);
    size_t src_index = GetNodeIndex(src, &node_id_to_node_index);
    size_t dst_index = GetNodeIndex(dst, &node_id_to_node_index);
    CHECK(src_index != dst_index);
    bool forward = true;
    if (src_index > dst_index) {
      forward = false;
      std::swap(src_index, dst_index);
    }

    LinkDataHelper& link_data_helper =
        src_to_dst_to_link_data[src_index][dst_index];
    link_data_helper.src_index = src_index;
    link_data_helper.dst_index = dst_index;
    link_data_helper.distance_hint = edge_data.distance_hint;

    CHECK(edge_data.load.size() == display_modes.size());
    if (forward) {
      link_data_helper.forward_load = edge_data.load;
      link_data_helper.forward_tooltip = edge_data.tooltip;
    } else {
      link_data_helper.reverse_load = edge_data.load;
      link_data_helper.reverse_tooltip = edge_data.tooltip;
    }

    link_data_set.insert(&link_data_helper);
  }

  for (const PathData& path_data : paths) {
    PathDataHelper path_data_helper;
    path_data_helper.label = path_data.label;
    path_data_helper.legend_label = path_data.legend_label;

    const net::GraphPath* path = path_data.path;
    path_data_helper.node_indices.reserve(path->link_sequence().size() + 1);

    const auto& links_on_path = path->link_sequence().links();
    for (const net::GraphLink* link : links_on_path) {
      const std::string& src = link->src();
      const std::string& dst = link->dst();

      size_t src_index = GetNodeIndex(src, &node_id_to_node_index);
      size_t dst_index = GetNodeIndex(dst, &node_id_to_node_index);
      // Each source will be added to the path's nodes, as well as the
      // destination of the last link.
      path_data_helper.node_indices.emplace_back(src_index);
      if (link == links_on_path.back()) {
        path_data_helper.node_indices.emplace_back(dst_index);
      }
    }

    std::set<size_t> node_index_set(path_data_helper.node_indices.begin(),
                                    path_data_helper.node_indices.end());
    if (node_index_set.size() != path_data_helper.node_indices.size()) {
      LOG(FATAL) << "Path with duplicate nodes";
    }

    all_paths.emplace_back(path_data_helper);
  }

  // Need to invert the node_id_to_node_index map. This will also sort the
  // NodeData instances by index.
  std::map<size_t, std::string> node_index_to_node_id;
  for (const auto& node_id_and_node_index : node_id_to_node_index) {
    const std::string& node_id = node_id_and_node_index.first;
    size_t node_index = node_id_and_node_index.second;
    node_index_to_node_id.emplace(node_index, node_id);
  }

  using json = nlohmann::json;
  json graph_json;
  for (const auto& node_index_and_node_id : node_index_to_node_id) {
    const std::string& node_id = node_index_and_node_id.second;
    json node_object;
    node_object["name"] = node_id;

    if (localizer) {
      double x, y;
      std::tie(x, y) = localizer(node_id);
      node_object["x"] = x;
      node_object["y"] = y;
      node_object["fixed"] = true;
    }

    graph_json["nodes"].push_back(node_object);
  }

  for (const LinkDataHelper* link_data : link_data_set) {
    json link_object;
    link_object["source"] = link_data->src_index;
    link_object["target"] = link_data->dst_index;
    link_object["forward_tooltip"] = link_data->forward_tooltip;
    link_object["reverse_tooltip"] = link_data->reverse_tooltip;
    link_object["distance_hint"] = link_data->distance_hint;
    for (double load : link_data->forward_load) {
      link_object["forward_load"].push_back(load);
    }
    for (double load : link_data->reverse_load) {
      link_object["reverse_load"].push_back(load);
    }
    graph_json["links"].push_back(link_object);
  }

  json paths_json;
  for (const PathDataHelper& path : all_paths) {
    json path_object;
    path_object["label"] = path.label;
    path_object["legend_label"] = path.legend_label;
    for (size_t node_index : path.node_indices) {
      path_object["nodes"].push_back(node_index);
    }
    paths_json.push_back(path_object);
  }

  static bool initialized = false;
  if (!initialized) {
    initialized = true;
    std::string graph_string(reinterpret_cast<const char*>(www_graph_html),
                             www_graph_html_size);
    ctemplate::StringToTemplateCache(kGraphKey, graph_string,
                                     ctemplate::DO_NOT_STRIP);
  }

  std::string graph_style_string(
      reinterpret_cast<const char*>(www_graph_style_html),
      www_graph_style_html_size);
  out->AddOrUpdateHeadElement("graph_style", graph_style_string);
  out->AddD3();

  ctemplate::TemplateDictionary dictionary("Graph");
  dictionary.SetValue(kGraphJSONKey, graph_json.dump(1));
  dictionary.SetValue(kPathJSONKey, paths_json.dump(1));
  for (const auto& display_mode : display_modes) {
    ctemplate::TemplateDictionary* sub_dict =
        dictionary.AddSectionDictionary(kDisplayModeSectionMarker);
    sub_dict->SetValue(kDisplayModeKey, display_mode.name);
  }

  CHECK(ctemplate::ExpandTemplate(kGraphKey, ctemplate::DO_NOT_STRIP,
                                  &dictionary, out->body()));
}

}  // namespace web
}  // namespace ncode
