#include "metrics.h"

#include <google/protobuf/repeated_field.h>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "gflags/gflags.h"
#include "../common/file.h"
#include "../common/strutil.h"

DEFINE_string(metrics_output, "metrics.out",
              "File where the metrics will be stored");
DEFINE_bool(per_metric_file, false,
            "Whether to store a single file per metric");

namespace ncode {
namespace metrics {

Distribution<double> ProtobufToDistribution(const PBDistribution& dist_pb) {
  std::vector<double> quantiles(dist_pb.quantiles().begin(),
                                dist_pb.quantiles().end());
  std::vector<double> cs(dist_pb.cumulative_sum_fractions().begin(),
                         dist_pb.cumulative_sum_fractions().end());
  std::vector<double> top_n(dist_pb.top_n().begin(), dist_pb.top_n().end());
  CHECK(quantiles.size() == cs.size())
      << "Quantiles / cumulative sums size mismatch";
  CHECK(quantiles.size() >= 2) << "Not enough quantiles";
  double min = quantiles.front();
  double max = quantiles.back();

  SummaryStats summary_stats;
  summary_stats.Reset(dist_pb.count(), dist_pb.sum(), dist_pb.sum_squared(),
                      min, max);
  Distribution<double> return_dist;
  return_dist.Reset(summary_stats, &cs, &quantiles, &top_n);
  return return_dist;
}

template <>
void ParseEntryFromProtobuf<uint64_t>(const PBMetricEntry& entry,
                                      Entry<uint64_t>* out) {
  out->timestamp = entry.timestamp();
  out->value = entry.uint64_value();
}

template <>
void ParseEntryFromProtobuf<uint32_t>(const PBMetricEntry& entry,
                                      Entry<uint32_t>* out) {
  out->timestamp = entry.timestamp();
  out->value = entry.uint32_value();
}

template <>
void ParseEntryFromProtobuf<bool>(const PBMetricEntry& entry,
                                  Entry<bool>* out) {
  out->timestamp = entry.timestamp();
  out->value = entry.bool_value();
}

template <>
void ParseEntryFromProtobuf<double>(const PBMetricEntry& entry,
                                    Entry<double>* out) {
  out->timestamp = entry.timestamp();
  out->value = entry.double_value();
}

template <>
void ParseEntryFromProtobuf<std::string>(const PBMetricEntry& entry,
                                         Entry<std::string>* out) {
  out->timestamp = entry.timestamp();
  out->value = entry.string_value();
}

template <>
void ParseEntryFromProtobuf<BytesBlob>(const PBMetricEntry& entry,
                                       Entry<BytesBlob>* out) {
  out->timestamp = entry.timestamp();
  out->value = entry.bytes_value();
}

template <>
void ParseEntryFromProtobuf<Distribution<double>>(
    const PBMetricEntry& entry, Entry<Distribution<double>>* out) {
  out->timestamp = entry.timestamp();
  out->value = ProtobufToDistribution(entry.distribution_value());
}

template <>
void SaveEntryToProtobuf<uint64_t>(const Entry<uint64_t>& entry,
                                   PBMetricEntry* out) {
  out->set_timestamp(entry.timestamp);
  out->set_uint64_value(entry.value);
}

template <>
void SaveEntryToProtobuf<uint32_t>(const Entry<uint32_t>& entry,
                                   PBMetricEntry* out) {
  out->set_timestamp(entry.timestamp);
  out->set_uint32_value(entry.value);
}

template <>
void SaveEntryToProtobuf<bool>(const Entry<bool>& entry, PBMetricEntry* out) {
  out->set_timestamp(entry.timestamp);
  out->set_bool_value(entry.value);
}

template <>
void SaveEntryToProtobuf<double>(const Entry<double>& entry,
                                 PBMetricEntry* out) {
  out->set_timestamp(entry.timestamp);
  out->set_double_value(entry.value);
}

template <>
void SaveEntryToProtobuf<std::string>(const Entry<std::string>& entry,
                                      PBMetricEntry* out) {
  out->set_timestamp(entry.timestamp);
  out->set_string_value(entry.value);
}

template <>
void SaveEntryToProtobuf<BytesBlob>(const Entry<BytesBlob>& entry,
                                    PBMetricEntry* out) {
  out->set_timestamp(entry.timestamp);
  *out->mutable_bytes_value() = entry.value;
}

OutputStream::OutputStream(const std::string& file) {
  fd_ = open(file.c_str(), O_WRONLY | O_TRUNC | O_CREAT,  // open mode
             S_IREAD | S_IWRITE | S_IRGRP | S_IROTH | S_ISUID);
  CHECK(fd_ > 0) << "Bad output file " << file;

  file_output_ = make_unique<google::protobuf::io::FileOutputStream>(fd_);
}

OutputStream::~OutputStream() {
  // close streams
  file_output_->Close();
  file_output_.reset();
  close(fd_);
}

void OutputStream::WriteBulk(const std::vector<PBMetricEntry>& entries,
                             uint32_t manifest_index) {
  std::lock_guard<std::mutex> lock(mu_);
  for (const auto& entry : entries) {
    WriteDelimitedTo(entry, manifest_index);
  }
}

void OutputStream::WriteSingle(const PBMetricEntry& entry,
                               uint32_t manifest_index) {
  std::lock_guard<std::mutex> lock(mu_);
  WriteDelimitedTo(entry, manifest_index);
}

// Writes a protobuf to the stream.
bool OutputStream::WriteDelimitedTo(const PBMetricEntry& entry,
                                    uint32_t manifest_index) {
  // Write the size and the manifest index. The index comes first.
  ::google::protobuf::io::CodedOutputStream coded_output(file_output_.get());
  coded_output.WriteVarint32(manifest_index);
  const int size = entry.ByteSize();
  coded_output.WriteVarint32(size);

  uint8_t* buffer = coded_output.GetDirectBufferForNBytesAndAdvance(size);
  if (buffer != nullptr) {
    // Optimization:  The message fits in one buffer, so use the faster
    // direct-to-array serialization path.
    entry.SerializeWithCachedSizesToArray(buffer);
  } else {
    // Slightly-slower path when the message is multiple buffers.
    entry.SerializeWithCachedSizes(&coded_output);
    if (coded_output.HadError()) {
      return false;
    }
  }

  return true;
}

InputStream::InputStream(const std::string& file) {
  fd_ = open(file.c_str(), O_RDONLY);
  CHECK(fd_ > 0) << "Bad input file " << file << ": " << strerror(errno);
  file_input_ = make_unique<google::protobuf::io::FileInputStream>(fd_);
}

InputStream::~InputStream() {
  // close streams
  file_input_->Close();
  file_input_.reset();
  close(fd_);
}

bool InputStream::ReadDelimitedHeaderFrom(uint32_t* manifest_index) {
  google::protobuf::io::CodedInputStream input(file_input_.get());

  // Read the manifest.
  if (!input.ReadVarint32(manifest_index)) {
    return false;
  }
  return true;
}

bool InputStream::SkipMessage() {
  google::protobuf::io::CodedInputStream input(file_input_.get());

  // Read the size.
  uint32_t size;
  if (!input.ReadVarint32(&size)) {
    return false;
  }

  return input.Skip(size);
}

bool InputStream::ReadDelimitedFrom(PBMetricEntry* message) {
  // We create a new coded stream for each message.
  google::protobuf::io::CodedInputStream input(file_input_.get());

  // Read the size.
  uint32_t size;
  if (!input.ReadVarint32(&size)) {
    return false;
  }

  // Tell the stream not to read beyond that size.
  google::protobuf::io::CodedInputStream::Limit limit = input.PushLimit(size);

  // Parse the message.
  if (!message->MergeFromCodedStream(&input)) {
    return false;
  }

  if (!input.ConsumedEntireMessage()) {
    return false;
  }

  // Release the limit.
  input.PopLimit(limit);

  return true;
}

void PopulateManifestEntryField(PBMetricField* field, uint64_t value) {
  field->set_type(PBMetricField::UINT64);
  field->set_uint64_value(value);
}

void PopulateManifestEntryField(PBMetricField* field, uint32_t value) {
  field->set_type(PBMetricField::UINT32);
  field->set_uint32_value(value);
}

void PopulateManifestEntryField(PBMetricField* field, bool value) {
  field->set_type(PBMetricField::BOOL);
  field->set_bool_value(value);
}

void PopulateManifestEntryField(PBMetricField* field,
                                const std::string& value) {
  field->set_type(PBMetricField::STRING);
  field->set_string_value(value);
}

MetricManager::MetricManager()
    : current_index_(std::numeric_limits<size_t>::max()),
      timestamp_provider_(make_unique<DefaultTimestampProvider>()) {}

size_t MetricManager::NextIndex() { return ++current_index_; }

void MetricManager::SetOutput(const std::string& output,
                              bool per_metric_files) {
  std::lock_guard<std::mutex> lock(mu_);
  if (output_stream_ || !output_directory_.empty()) {
    LOG(ERROR) << "Output already set. Will ignore";
    return;
  }

  // If any of the metrics are already locked setting the stream will result in
  // broken output.
  for (const auto& metric : all_metrics_) {
    CHECK(!metric->stream_locked());
  }

  if (per_metric_files) {
    File::RecursivelyCreateDir(output, 0700);
    output_directory_ = output;
    for (const auto& metric : all_metrics_) {
      std::string local_output = StrCat(output_directory_, "/", metric->id());
      auto local_output_stream = make_unique<OutputStream>(local_output);
      metric->SetLocalOutputStream(std::move(local_output_stream));
    }
  } else {
    output_stream_ = make_unique<OutputStream>(output);
  }
}

const TimestampProviderInterface* MetricManager::timestamp_provider() const {
  return timestamp_provider_.get();
}

const TimestampProviderInterface* MetricBase::timestamp_provider() const {
  return parent_manager_->timestamp_provider();
}

void MetricBase::SetLocalOutputStream(
    std::unique_ptr<OutputStream> output_stream) {
  CHECK(!local_output_stream_);
  CHECK(local_current_index_ == std::numeric_limits<size_t>::max());
  CHECK(!stream_locked_);
  local_output_stream_ = std::move(output_stream);
}

OutputStream* MetricBase::OutputStreamOrNull() {
  // Regardless of whether there was a stream or not we cannot allow further
  // changes to the stream, as it may result in missing manifest entries when
  // parsing.
  stream_locked_ = true;

  if (local_output_stream_) {
    return local_output_stream_.get();
  }

  OutputStream* output_stream = parent_manager_->OutputStreamOrNull();
  if (output_stream != nullptr) {
    return output_stream;
  }

  return nullptr;
}

size_t MetricBase::NextIndex() {
  if (local_output_stream_) {
    return ++local_current_index_;
  }

  return parent_manager_->NextIndex();
}

MetricHandleBase::MetricHandleBase(bool has_callback, MetricBase* parent_metric)
    : has_callback_(has_callback),
      metric_index_(0),
      parent_metric_(parent_metric) {}

std::string MetricHandleBase::TimestampToString(uint64_t timestamp) const {
  return parent_metric_->timestamp_provider()->TimestampToString(timestamp);
}

void MetricManager::PersistAllMetrics() {
  for (const auto& metric_ptr : all_metrics_) {
    metric_ptr->PersistAllHandles();
  }
}

void MetricManager::PollAllMetrics() {
  for (const auto& metric_ptr : all_metrics_) {
    metric_ptr->PollAllHandles();
  }
}

MetricManager::~MetricManager() { PersistAllMetrics(); }

MetricManager* DefaultMetricManager() {
  static MetricManager default_manager;
  return &default_manager;
}

void InitMetrics() {
  MetricManager* manager = DefaultMetricManager();
  manager->SetOutput(FLAGS_metrics_output, FLAGS_per_metric_file);
}

void PopulateManifestEntryType(PBManifestEntry* out, uint64_t* dummy) {
  Unused(dummy);
  out->set_type(PBManifestEntry::UINT64);
}

void PopulateManifestEntryType(PBManifestEntry* out, uint32_t* dummy) {
  Unused(dummy);
  out->set_type(PBManifestEntry::UINT32);
}

void PopulateManifestEntryType(PBManifestEntry* out, bool* dummy) {
  Unused(dummy);
  out->set_type(PBManifestEntry::BOOL);
}

void PopulateManifestEntryType(PBManifestEntry* out, std::string* dummy) {
  Unused(dummy);
  out->set_type(PBManifestEntry::STRING);
}

void PopulateManifestEntryType(PBManifestEntry* out, double* dummy) {
  Unused(dummy);
  out->set_type(PBManifestEntry::DOUBLE);
}

void PopulateManifestEntryType(PBManifestEntry* out, BytesBlob* dummy) {
  Unused(dummy);
  out->set_type(PBManifestEntry::BYTES);
}

DefaultMetricManagerPoller::DefaultMetricManagerPoller(
    std::chrono::milliseconds period, EventQueue* event_queue)
    : EventConsumer("metric_poller", event_queue), period_(period) {
  EnqueueNext();
}

void DefaultMetricManagerPoller::HandleEvent() {
  ncode::metrics::DefaultMetricManager()->PollAllMetrics();
  EnqueueNext();
}

}  // namespace metrics
}  // namespace ncode
