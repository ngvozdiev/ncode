#include <gflags/gflags.h>
#include <cstdio>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "../web/mongoose/mongoose.h"
#undef LOG

#include "../common/common.h"
#include "../common/substitute.h"
#include "../common/strutil.h"
#include "../common/file.h"
#include "../common/logging.h"
#include "../grapher/grapher.h"
#include "../web/web_page.h"
#include "metrics_parser.h"

DEFINE_string(python_plot_output, "",
              "If set will store the data for all plots there");
DEFINE_string(data_dump_location, "",
              "If specified all data will be dumped to this file");
DEFINE_string(input, "", "The metrics file or directory of metrics files.");
DEFINE_uint64(port_num, 8080, "Port to listen on");

using namespace ncode;

static constexpr char kDefaultTableId[] = "table";
static constexpr char kTitleVariableName[] = "title";
static constexpr char kXLabelVariableName[] = "xlabel";
static constexpr char kYLabelVariableName[] = "ylabel";
static constexpr char kXScaleVariableName[] = "xscale";
static constexpr char kYScaleVariableName[] = "yscale";
static constexpr char kLimitingVariableName[] = "limiting";
static constexpr char kCollapseVariableName[] = "collapse";
static constexpr char kManifestIdsVariableName[] = "manifestids";
static constexpr char kBinSizeVariableName[] = "binsize";
static constexpr char kActiveIndexVariableName[] = "i";

// Dumps data to FLAGS_data_dump_location.
static void DumpData(
    const std::map<std::pair<std::string, std::string>,
                   std::vector<std::pair<uint64_t, double>>>& data) {
  if (FLAGS_data_dump_location.empty()) {
    return;
  }

  std::ofstream out(FLAGS_data_dump_location, std::ofstream::trunc);
  for (const auto& ids_and_data : data) {
    const std::pair<std::string, std::string>& ids = ids_and_data.first;
    const std::vector<std::pair<uint64_t, double>>& values =
        ids_and_data.second;

    std::function<std::string(const std::pair<uint64_t, double>&)> f = [](
        const std::pair<uint64_t, double>& timestamp_and_value) {
      return ncode::StrCat(timestamp_and_value.first, " ",
                           timestamp_and_value.second);
    };

    std::string to_write = ncode::StrCat(ids.first, " ", ids.second, " ");
    ncode::Join(values.begin(), values.end(), " ", f, &to_write);
    out << to_write << "\n";
  }
  LOG(ERROR) << "Dumped " << data.size() << " data";
  out.close();
}

struct FileAndManifest {
  FileAndManifest(FileAndManifest&& other)
      : filename(std::move(other.filename)),
        size_mb(other.size_mb),
        manifest(std::move(other.manifest)) {}

  FileAndManifest(const std::string& filename, uint64_t size_mb,
                  metrics::parser::Manifest&& manifest)
      : filename(filename), size_mb(size_mb), manifest(std::move(manifest)) {}

  std::string filename;
  uint64_t size_mb;
  metrics::parser::Manifest manifest;
};

// State associated with the server.
class ServerState {
 public:
  ServerState(const std::string& file_or_dir) {
    bool is_dir;
    CHECK(ncode::File::FileOrDirectory(file_or_dir, &is_dir));

    std::vector<std::string> files;
    if (is_dir) {
      files = ncode::Glob(ncode::StrCat(file_or_dir, "/*"));
    } else {
      files.emplace_back(file_or_dir);
    }

    for (const std::string& file : files) {
      uint64_t file_size_mb = ncode::File::FileSizeOrDie(file) / 1000 / 1000;

      metrics::parser::MetricsParser parser(file);
      files_.emplace_back(file, file_size_mb, parser.ParseManifest());
      LOG(INFO) << "Parsed " << file;
    }
  }

  std::unique_ptr<web::TemplatePage> GetPage() {
    std::unique_ptr<web::TemplatePage> page = web::GetDefaultTemplate();
    page->AddNavigationEntry({"File List", "/index", false});
    return page;
  }

  void FileListToTable(web::HtmlPage* out) {
    web::HtmlTable table(kDefaultTableId,
                         {"File Name", "File Size", "Total num entries"});
    for (size_t i = 0; i < files_.size(); ++i) {
      const FileAndManifest& file_and_manifest = files_[i];

      std::string link =
          Substitute("/manifest?$0=$1", kActiveIndexVariableName, i);
      uint64_t file_size_mb = file_and_manifest.size_mb;
      std::string file_name = File::ExtractFileName(file_and_manifest.filename);
      uint64_t num_entries = file_and_manifest.manifest.TotalEntryCount();
      table.AddRow({web::GetLink(link, file_name), std::to_string(file_size_mb),
                    std::to_string(num_entries)});
    }

    table.ToHtml(out);
  }

  void PlotIdTimeSeries(const std::string& input_file,
                        const std::set<uint32_t>& manifest_ids,
                        const grapher::PlotParameters2D& plot_params,
                        web::HtmlPage* out) {
    if (manifest_ids.empty()) {
      return;
    }

    uint64_t max = std::numeric_limits<uint64_t>::max();
    auto return_map = metrics::parser::SimpleParseNumericData(
        input_file, manifest_ids, 0, max, 0);
    DumpData(return_map);
    PlotTimeSeries(return_map, plot_params, out);
  }

  void PlotIdCDF(const std::string& input_file,
                 const std::set<uint32_t>& manifest_ids, bool limiting,
                 bool collapse, const grapher::PlotParameters1D& plot_params,
                 web::HtmlPage* out) {
    if (manifest_ids.empty()) {
      return;
    }

    uint64_t max = std::numeric_limits<uint64_t>::max();
    auto return_map = metrics::parser::SimpleParseNumericData(
        input_file, manifest_ids, 0, max, limiting ? max : 0);
    DumpData(return_map);
    PlotCDF(return_map, collapse, plot_params, out);
  }

  const std::vector<FileAndManifest>& files() const { return files_; }

 private:
  void PlotTimeSeries(
      const std::map<std::pair<std::string, std::string>,
                     std::vector<std::pair<uint64_t, double>>>& data,
      const grapher::PlotParameters2D& plot_params, web::HtmlPage* out) {
    if (data.empty()) {
      StrAppend(out->body(), web::GetDiv("No numeric data"));
      return;
    }

    std::vector<grapher::DataSeries2D> all_data_to_plot;
    for (const auto& label_and_values : data) {
      const std::string& label_id = label_and_values.first.second;
      const std::vector<std::pair<uint64_t, double>>& values =
          label_and_values.second;

      grapher::DataSeries2D to_plot;
      to_plot.label = label_id;

      for (const auto& timestamp_and_value : values) {
        uint64_t timestamp = timestamp_and_value.first;
        double value = timestamp_and_value.second;
        to_plot.data.emplace_back(timestamp, value);
      }
      all_data_to_plot.emplace_back(std::move(to_plot));
    }

    grapher::HtmlGrapher html_grapher(out);
    html_grapher.PlotLine(plot_params, all_data_to_plot);

    if (!FLAGS_python_plot_output.empty()) {
      grapher::PythonGrapher python_grapher(FLAGS_python_plot_output);
      python_grapher.PlotLine(plot_params, all_data_to_plot);
      LOG(INFO) << "Saved script to plot data at " << FLAGS_python_plot_output;
    }
  }

  // Plots a CDF of the values in one (or more) metrics. The input map is as
  // returned by any of the query functions in metrics_parser. If the collapse
  // argument is true only one curve will be plotted that will combine all sets
  // of  values regardless of the metric or set of fields.
  void PlotCDF(const std::map<std::pair<std::string, std::string>,
                              std::vector<std::pair<uint64_t, double>>>& data,
               bool collapse, const grapher::PlotParameters1D& plot_params,
               web::HtmlPage* out) {
    if (data.empty()) {
      StrAppend(out->body(), web::GetDiv("No numeric data"));
      return;
    }

    std::vector<grapher::DataSeries1D> all_data_to_plot;
    if (collapse) {
      grapher::DataSeries1D to_plot;
      std::vector<double> all_values;
      for (const auto& label_and_values : data) {
        const std::vector<std::pair<uint64_t, double>>& values =
            label_and_values.second;
        for (const auto& timestamp_and_value : values) {
          double value = timestamp_and_value.second;
          all_values.emplace_back(value);
        }
      }

      to_plot.data = std::move(all_values);
      all_data_to_plot.emplace_back(std::move(to_plot));
    } else {
      for (const auto& label_and_values : data) {
        const std::string& label_id = label_and_values.first.second;
        const std::vector<std::pair<uint64_t, double>>& values =
            label_and_values.second;
        LOG(INFO) << values.size();

        grapher::DataSeries1D to_plot;
        for (const auto& timestamp_and_value : values) {
          double value = timestamp_and_value.second;
          to_plot.data.emplace_back(value);
        }

        to_plot.label = label_id;
        all_data_to_plot.emplace_back(std::move(to_plot));
      }
    }

    grapher::HtmlGrapher html_grapher(out);
    html_grapher.PlotCDF(plot_params, all_data_to_plot);

    if (!FLAGS_python_plot_output.empty()) {
      grapher::PythonGrapher python_grapher(FLAGS_python_plot_output);
      python_grapher.PlotCDF(plot_params, all_data_to_plot);
      LOG(INFO) << "Saved script to plot data at " << FLAGS_python_plot_output;
    }
  }

  std::vector<FileAndManifest> files_;
};

// Given a list of the form "[1,2,3,4]" will return a set of the integer values.
static std::set<uint32_t> ExtractManifestIds(http_message* hm,
                                             web::HtmlPage* out);

static std::string GetLinkForMetricId(const std::string& id,
                                      uint32_t file_index) {
  std::string location =
      StrCat("metric?id=", id, "&", kActiveIndexVariableName, "=", file_index);
  return web::GetLink(location, id);
}

static std::vector<std::string> ManifestToHtml(
    const metrics::parser::Manifest& manifest, uint32_t file_index,
    web::HtmlPage* out) {
  std::vector<std::string> ids;
  web::HtmlTable table = manifest.FullToTable(
      kDefaultTableId, [file_index, &ids](const std::string& id) {
        ids.emplace_back(id);
        return GetLinkForMetricId(id, file_index);
      });

  if (ids.size() > 1) {
    table.ToHtml(out);
  }

  return ids;
}

static void MetricToHtml(const metrics::parser::Manifest& manifest,
                         const std::string& metric_id, uint32_t file_index,
                         web::HtmlPage* out) {
  StrAppend(out->body(), web::GetP(StrCat("Metric is ", metric_id)));

  web::HtmlTable table = manifest.ToTable(metric_id, kDefaultTableId);
  // Will assume that the 0-th column is the index.
  table.AddSelect("time_series_form_0", 0);
  table.AddSelect("cdf_form_0", 0);
  table.ToHtml(out);

  web::HtmlForm time_series_form("plot_manifest", "time_series_form", false);
  auto manifest_ids_field = make_unique<web::HtmlFormTextInput>(
      kManifestIdsVariableName, "Manifest Ids");
  manifest_ids_field->required = true;

  time_series_form.AddField(std::move(manifest_ids_field));
  auto title_field =
      make_unique<web::HtmlFormTextInput>(kTitleVariableName, "Plot title");
  time_series_form.AddField(std::move(title_field));
  time_series_form.AddField(
      make_unique<web::HtmlFormTextInput>(kXLabelVariableName, "X label"));
  time_series_form.AddField(
      make_unique<web::HtmlFormTextInput>(kXScaleVariableName, "X scale"));
  time_series_form.AddField(
      make_unique<web::HtmlFormTextInput>(kYLabelVariableName, "Y label"));
  time_series_form.AddField(
      make_unique<web::HtmlFormTextInput>(kYScaleVariableName, "Y scale"));
  time_series_form.AddField(
      make_unique<web::HtmlFormTextInput>(kBinSizeVariableName, "Bin size"));
  time_series_form.AddField(make_unique<web::HtmlFormHiddenInput>(
      kActiveIndexVariableName, std::to_string(file_index)));

  web::AccordionStart("Time series plot", out);
  time_series_form.ToHtml(out);
  web::AccordionEnd(out);

  web::HtmlForm cdf_form("plot_manifest_cdf", "cdf_form", false);
  manifest_ids_field = make_unique<web::HtmlFormTextInput>(
      kManifestIdsVariableName, "Manifest Ids");
  manifest_ids_field->required = true;

  cdf_form.AddField(std::move(manifest_ids_field));
  title_field =
      make_unique<web::HtmlFormTextInput>(kTitleVariableName, "Plot title");
  cdf_form.AddField(std::move(title_field));
  cdf_form.AddField(
      make_unique<web::HtmlFormTextInput>(kXLabelVariableName, "Data label"));
  cdf_form.AddField(
      make_unique<web::HtmlFormTextInput>(kXScaleVariableName, "Data scale"));
  cdf_form.AddField(make_unique<web::HtmlFormCheckboxInput>(
      kLimitingVariableName, "Limiting (only consider last value of each)"));
  cdf_form.AddField(make_unique<web::HtmlFormCheckboxInput>(
      kCollapseVariableName,
      "Collapse (only plot 1 curve with all values combined)"));
  cdf_form.AddField(make_unique<web::HtmlFormHiddenInput>(
      kActiveIndexVariableName, std::to_string(file_index)));

  web::AccordionStart("CDF plot", out);
  cdf_form.ToHtml(out);
  web::AccordionEnd(out);
}

static void SendHttpHeader(mg_connection* connection) {
  mg_printf(connection, "%s",
            "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");
}

static void SendEmptyChunk(struct mg_connection* connection) {
  mg_send_http_chunk(connection, "", 0);
}

static void SendData(const std::string& data, mg_connection* connection) {
  SendHttpHeader(connection);
  mg_send_http_chunk(connection, data.c_str(), data.size());
  SendEmptyChunk(connection);
}

static constexpr size_t kLargeBufferSize = 1 << 20;

// Extracts a get/post variable from the http message or returns "".
static std::string ExtractVariable(const std::string& id, http_message* hm,
                                   bool unescape = false) {
  // Used for big POST variables.
  std::unique_ptr<char[]> heap_buffer;

  // Used for smaller GET variables.
  char var[100] = {0};
  int res;

  // Will first try the query string (GET), if that fails will try the body
  // (POST).
  res = mg_get_http_var(&hm->query_string, id.c_str(), var, sizeof(var));
  if (res == -2) {
    LOG(ERROR) << "Buffer too small (GET) for " << id;
  }

  if (res == 0 || res == -1 || res == -2) {
    res = mg_get_http_var(&hm->body, id.c_str(), var, sizeof(var));
    if (res == -2) {
      // Clearly we need a bigger buffer. Will allocate one on the heap.
      heap_buffer = std::unique_ptr<char[]>(new char[kLargeBufferSize]);
      res = mg_get_http_var(&hm->body, id.c_str(), heap_buffer.get(),
                            kLargeBufferSize);
      if (res == -2) {
        LOG(FATAL) << "Variable too large even for kLargeBufferSize of "
                   << kLargeBufferSize;
      }
    }
  }

  if (res == 0 || res == -1) {
    return "";
  }

  std::string out;
  if (heap_buffer) {
    out = std::string(heap_buffer.get());
  } else {
    out = std::string(var);
  }

  if (unescape) {
    std::string unescaped;
    CHECK(WebSafeBase64Unescape(out, &unescaped));
    return unescaped;
  }

  return out;
}

#define EXTRACT_AND_SET(var_string, var_name)        \
  {                                                  \
    std::string v = ExtractVariable(var_string, hm); \
    if (!v.empty()) {                                \
      parameters.var_name = v;                       \
    }                                                \
  }

#define EXTRACT_AND_SET_DOUBLE(var_string, var_name) \
  {                                                  \
    std::string v = ExtractVariable(var_string, hm); \
    double v_double = 0.0;                           \
    if (!ncode::safe_strtod(v, &v_double)) {         \
      LOG(ERROR) << "Bad double";                    \
    }                                                \
    if (!v.empty()) {                                \
      parameters.var_name = v_double;                \
    }                                                \
  }

static ncode::grapher::PlotParameters2D Extract2DPlotParameters(
    http_message* hm) {
  ncode::grapher::PlotParameters2D parameters;

  EXTRACT_AND_SET(kTitleVariableName, title);
  EXTRACT_AND_SET(kXLabelVariableName, x_label);
  EXTRACT_AND_SET(kYLabelVariableName, y_label);
  EXTRACT_AND_SET_DOUBLE(kYScaleVariableName, y_scale);
  EXTRACT_AND_SET_DOUBLE(kXScaleVariableName, x_scale);
  EXTRACT_AND_SET_DOUBLE(kBinSizeVariableName, x_bin_size);

  return parameters;
}

static ncode::grapher::PlotParameters1D Extract1DPlotParameters(
    http_message* hm) {
  ncode::grapher::PlotParameters1D parameters;

  EXTRACT_AND_SET(kTitleVariableName, title);
  EXTRACT_AND_SET(kXLabelVariableName, data_label);
  EXTRACT_AND_SET_DOUBLE(kXScaleVariableName, scale);
  return parameters;
}

static uint32_t ExtractIndexOrZero(http_message* hm) {
  std::string var = ExtractVariable(kActiveIndexVariableName, hm);
  uint32_t i;
  if (!ncode::safe_strtou32(var, &i)) {
    return 0;
  }
  return i;
}

static bool ExtractBoolOrFalse(const std::string& value) {
  bool return_value;
  if (!ncode::safe_strtob(value, &return_value)) {
    return false;
  }
  return return_value;
}

static void Handler(mg_connection* connection, int event, void* p) {
  ServerState* server_state =
      reinterpret_cast<ServerState*>(connection->mgr->user_data);
  CHECK(server_state != nullptr);

  if (event == MG_EV_HTTP_REQUEST) {
    http_message* hm = (http_message*)p;
    std::unique_ptr<web::TemplatePage> page = server_state->GetPage();
    uint32_t i = ExtractIndexOrZero(hm);

    const std::vector<FileAndManifest>& files = server_state->files();
    CHECK(i < files.size()) << "Index out of range";
    const FileAndManifest& file = files[i];

    if (mg_vcmp(&hm->uri, "/index") == 0) {
      server_state->FileListToTable(page.get());

      SendData(page->Construct(), connection);
    } else if (mg_vcmp(&hm->uri, "/manifest") == 0) {
      std::vector<std::string> ids =
          ManifestToHtml(file.manifest, i, page.get());
      if (ids.size() == 1) {
        MetricToHtml(file.manifest, ids.front(), i, page.get());
      }

      SendData(page->Construct(), connection);
    } else if (mg_vcmp(&hm->uri, "/metric") == 0) {
      std::string metric_id = ExtractVariable("id", hm);
      if (metric_id.empty()) {
        SendData("No metric id", connection);
      } else {
        MetricToHtml(file.manifest, metric_id, i, page.get());
        SendData(page->Construct(), connection);
      }
    } else if (mg_vcmp(&hm->uri, "/plot_manifest") == 0) {
      server_state->PlotIdTimeSeries(file.filename,
                                     ExtractManifestIds(hm, page.get()),
                                     Extract2DPlotParameters(hm), page.get());

      SendData(page->Construct(), connection);
    } else if (mg_vcmp(&hm->uri, "/plot_manifest_cdf") == 0) {
      std::string limiting = ExtractVariable(kLimitingVariableName, hm);
      std::string collapse = ExtractVariable(kCollapseVariableName, hm);
      server_state->PlotIdCDF(file.filename, ExtractManifestIds(hm, page.get()),
                              ExtractBoolOrFalse(limiting),
                              ExtractBoolOrFalse(collapse),
                              Extract1DPlotParameters(hm), page.get());

      SendData(page->Construct(), connection);
    } else {
      SendData("Unknown URL", connection);
    }
  }
}

static std::set<uint32_t> ExtractManifestIds(http_message* hm,
                                             web::HtmlPage* out) {
  std::string ids_string = ExtractVariable(kManifestIdsVariableName, hm);
  if (ids_string.empty()) {
    StrAppend(out->body(), web::GetDiv("Empty manifest ids"));
    return std::set<uint32_t>();
  }

  std::vector<std::string> pieces =
      ncode::Split(ids_string.substr(1, ids_string.size() - 2), ",");
  std::set<uint32_t> manifest_ids;
  uint32_t value;
  for (const std::string& piece : pieces) {
    if (!ncode::safe_strtou32(piece, &value)) {
      StrAppend(out->body(), web::GetDiv("Unable to parse manifest ids"));
      return std::set<uint32_t>();
    }
    manifest_ids.emplace(value);
  }

  return manifest_ids;
}

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  CHECK(!FLAGS_input.empty()) << "Empty input file";

  struct mg_mgr mgr;
  struct mg_connection* nc;

  ServerState server_state(FLAGS_input);
  mg_mgr_init(&mgr, &server_state);
  nc = mg_bind(&mgr, std::to_string(FLAGS_port_num).c_str(), Handler);

  mg_set_protocol_http_websocket(nc);
  LOG(INFO) << "Starting web server on port " << FLAGS_port_num;

  while (true) {
    mg_mgr_poll(&mgr, 100);
  }
  mg_mgr_free(&mgr);
}
