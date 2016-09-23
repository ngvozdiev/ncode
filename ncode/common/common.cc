#include "common.h"

#include <glob.h>
#include <string>

namespace ncode {
namespace common {

std::vector<std::string> Glob(const std::string& pat) {
  glob_t glob_result;
  glob(pat.c_str(), GLOB_TILDE, NULL, &glob_result);
  std::vector<std::string> ret;
  for (unsigned int i = 0; i < glob_result.gl_pathc; ++i) {
    ret.push_back(std::string(glob_result.gl_pathv[i]));
  }
  globfree(&glob_result);
  return ret;
}

void Bin(size_t bin_size, std::vector<std::pair<double, double>>* data) {
  CHECK(bin_size != 0);
  if (bin_size == 1) {
    return;
  }

  double bin_total = 0;
  size_t bin_index = 0;
  for (size_t i = 0; i < data->size(); ++i) {
    if (i != 0 && i % bin_size == 0) {
      double mean = bin_total / bin_size;
      double bin_start = (*data)[i - bin_size].first;
      (*data)[bin_index++] = {bin_start, mean};
      bin_total = 0;
    }

    bin_total += (*data)[i].second;
  }

  size_t remainder = data->size() % bin_size;
  size_t base = (data->size() / bin_size) * bin_size;
  if (remainder == 0) {
    data->resize(bin_index);
    return;
  }

  double mean = bin_total / remainder;
  double bin_start = (*data)[base].first;
  (*data)[bin_index++] = {bin_start, mean};
  data->resize(bin_index);
}

uint32_t ghtonl(uint32_t x) {
  union {
    uint32_t result;
    uint8_t result_array[4];
  };
  result_array[0] = static_cast<uint8_t>(x >> 24);
  result_array[1] = static_cast<uint8_t>((x >> 16) & 0xFF);
  result_array[2] = static_cast<uint8_t>((x >> 8) & 0xFF);
  result_array[3] = static_cast<uint8_t>(x & 0xFF);
  return result;
}

void SummaryStats::Add(double value) {
  static double max_add_value =
      std::pow(std::numeric_limits<double>::max(), 0.5);
  CHECK(value < max_add_value) << "Value too large";

  double value_squared = value * value;
  if ((value_squared < 0.0) == (sum_squared_ < 0.0) &&
      std::abs(value_squared) >
          std::numeric_limits<double>::max() - std::abs(sum_squared_)) {
    LOG(FATAL) << "Addition overflowing";
  }

  if (value < min_) {
    min_ = value;
  }

  if (value > max_) {
    max_ = value;
  }

  ++count_;
  sum_ += value;
  sum_squared_ += value * value;
}

void SummaryStats::Reset() {
  sum_ = 0;
  count_ = 0;
  sum_squared_ = 0;
  min_ = std::numeric_limits<double>::max();
  max_ = std::numeric_limits<double>::min();
}

double SummaryStats::min() const {
  CHECK(count_ > 0) << "No values yet";
  return min_;
}

double SummaryStats::max() const {
  CHECK(count_ > 0) << "No values yet";
  return max_;
}

double SummaryStats::mean() const {
  CHECK(count_ > 0) << "No values yet";
  return sum_ / count_;
}

double SummaryStats::var() const {
  double m = mean();
  return sum_squared_ / count_ - m * m;
}

void SummaryStats::Reset(size_t count, double sum, double sum_squared,
                         double min, double max) {
  count_ = count;
  sum_ = sum;
  sum_squared_ = sum_squared;
  min_ = min;
  max_ = max;
}

bool ExpDetect(const std::vector<double>& values, double power,
               double tolerance, size_t min_len) {
  if (min_len == 0) {
    return true;
  }

  if (values.size() == 0) {
    return false;
  }

  if (min_len == 1 && values.size() > 0) {
    return true;
  }

  size_t num_values = values.size();
  for (size_t i = 0; i < num_values - 1; ++i) {
    double starting_value = values[i];
    double next_num = starting_value * power;
    size_t len = 1;

    for (size_t j = i + 1; j < num_values; ++j) {
      double next_value = values[j];
      if (std::fabs(next_num - static_cast<double>(next_value)) <= tolerance) {
        ++len;
        next_num *= power;

        if (j == num_values - 1 && len >= min_len) {
          return true;
        }

        continue;
      }

      // End the progression
      if (len >= min_len) {
        return true;
      }

      break;
    }
  }

  return false;
}

void ThresholdEnforcerPolicy::set_empty_threshold_absolute(
    double empty_threshold_absolute) {
  CHECK(empty_threshold_absolute >= 0)
      << "Absolute threshold should be a positive number";
  empty_threshold_absolute_ = empty_threshold_absolute;
}

void ThresholdEnforcerPolicy::set_threshold_absolute(
    double threshold_absolute) {
  CHECK(threshold_absolute >= 0)
      << "Absolute threshold should be a positive number";
  threshold_absolute_ = threshold_absolute;
}

void ThresholdEnforcerPolicy::set_threshold_relative_to_current(
    double threshold_relative_to_current) {
  CHECK(threshold_relative_to_current >= 0 &&
        threshold_relative_to_current <= 1)
      << "Relative threshold should be in [0-1]";
  threshold_relative_to_current_ = threshold_relative_to_current;
}

void ThresholdEnforcerPolicy::set_threshold_relative_to_total(
    double threshold_relative_to_total) {
  CHECK(threshold_relative_to_total >= 0 && threshold_relative_to_total <= 1)
      << "Relative threshold should be in [0-1]";
  threshold_relative_to_total_ = threshold_relative_to_total;
}

double ThresholdEnforcerPolicy::empty_threshold_absolute() const {
  return empty_threshold_absolute_;
}

double ThresholdEnforcerPolicy::threshold_absolute() const {
  return threshold_absolute_;
}

double ThresholdEnforcerPolicy::threshold_relative_to_current() const {
  return threshold_relative_to_current_;
}

double ThresholdEnforcerPolicy::threshold_relative_to_total() const {
  return threshold_relative_to_total_;
}

void TimeoutPolicy::set_timeout_penalty(uint64_t timeout_penalty) {
  timeout_penalty_ = timeout_penalty;
}

uint64_t TimeoutPolicy::timeout_penalty_lookback() const {
  return timeout_penalty_lookback_;
}

void TimeoutPolicy::set_timeout_penalty_lookback(
    uint64_t timeout_penalty_lookback) {
  timeout_penalty_lookback_ = timeout_penalty_lookback;
}

bool TimeoutPolicy::timeout_penalty_cumulative() const {
  return timeout_penalty_cumulative_;
}

void TimeoutPolicy::set_timeout_penalty_cumulative(
    bool timeout_penalty_cumulative) {
  timeout_penalty_cumulative_ = timeout_penalty_cumulative;
}

}  // namespace common
}  // namespace ncode
