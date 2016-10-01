#include "common.h"
#include "map_util.h"
#include "gtest/gtest.h"

namespace ncode {
namespace {

static constexpr size_t kMillion = 1000000;

TEST(Percentiles, BadValues) {
  std::vector<size_t> percentiles = Percentiles<size_t>(nullptr);
  ASSERT_TRUE(percentiles.empty());
}

TEST(Percentiles, NoValues) {
  std::vector<size_t> values;
  std::vector<size_t> percentiles = Percentiles(&values);
  ASSERT_TRUE(percentiles.empty());
}

TEST(Percentiles, SingleValue) {
  std::vector<size_t> values = {1};
  std::vector<size_t> percentiles = Percentiles(&values);
  ASSERT_EQ(101ul, percentiles.size());
  for (size_t i = 0; i < 101; ++i) {
    ASSERT_EQ(1ul, percentiles[i]);
  }
}

TEST(Percentiles, RandomValue) {
  std::vector<double> values;
  for (size_t i = 0; i < kMillion; ++i) {
    values.emplace_back(std::rand() / static_cast<double>(RAND_MAX));
  }

  std::sort(values.begin(), values.end());
  double min = values.front();
  double max = values.back();
  double med = values[kMillion / 2];

  std::vector<double> percentiles = Percentiles(&values);
  ASSERT_EQ(101, percentiles.size());
  ASSERT_EQ(min, percentiles.front());
  ASSERT_EQ(max, percentiles.back());
  ASSERT_EQ(med, percentiles[50]);
}

TEST(Percentiles, RandomValueTenPercentiles) {
  std::vector<double> values;
  for (size_t i = 0; i < kMillion; ++i) {
    values.emplace_back(std::rand() / static_cast<double>(RAND_MAX));
  }

  std::vector<double> percentiles = Percentiles(&values, 10);
  ASSERT_EQ(11, percentiles.size());
}

TEST(Bin, BadArgument) {
  std::vector<std::pair<double, double>> values = {};
  ASSERT_DEATH(Bin(0, &values), ".*");
}

TEST(Bin, Empty) {
  std::vector<std::pair<double, double>> values = {};
  Bin(10, &values);
  ASSERT_TRUE(values.empty());
}

TEST(Bin, ShortList) {
  std::vector<std::pair<double, double>> values = {
      {1.0, 1.0}, {2.0, 20.0}, {4.0, 10.0}};
  std::vector<std::pair<double, double>> binned_values = {{1.0, 31.0 / 3}};
  Bin(10, &values);

  // Bin size is 10, but there are only 3 values.
  ASSERT_EQ(binned_values, values);
}

TEST(Bin, SingleBin) {
  std::vector<std::pair<double, double>> values = {
      {1.0, 1.0}, {2.0, 20.0}, {4.0, 10.0}, {18.0, 16.0}, {18.5, 8.0}};
  std::vector<std::pair<double, double>> binned_values = {{1.0, 31.0 / 3},
                                                          {18.0, 24.0 / 2}};
  Bin(3, &values);

  // One bin, the remaining values are binned in a bin of their own.
  ASSERT_EQ(binned_values, values);
}

TEST(Bin, MultiBin) {
  std::vector<std::pair<double, double>> values = {
      {1.0, 1.0}, {2.0, 20.0}, {4.0, 10.0}, {18.0, 16.0}, {18.5, 8.0}};
  std::vector<std::pair<double, double>> binned_values = {
      {1.0, 21.0 / 2}, {4.0, 26.0 / 2}, {18.5, 8.0}};
  Bin(2, &values);

  // Two bins.
  ASSERT_EQ(binned_values, values);
}

static void AssertAlmostEqual(const std::vector<double> v1,
                              const std::vector<double> v2) {
  ASSERT_EQ(v1.size(), v2.size());

  for (size_t i = 0; i < v1.size(); ++i) {
    ASSERT_NEAR(v1[i], v2[i], 0.0000001);
  }
}

TEST(CumulativeFractions, RandomValueTenFractions) {
  std::vector<double> values;
  double sum = 0;
  for (size_t i = 0; i < kMillion; ++i) {
    values.emplace_back(std::rand() / static_cast<double>(RAND_MAX));
    sum += values.back();
  }

  std::vector<size_t> indices = {0,      100000, 200000, 300000, 400000, 500000,
                                 599999, 699999, 799999, 899999, 999999};

  std::vector<double> values_sorted = values;
  std::sort(values_sorted.begin(), values_sorted.end());

  std::vector<double> model(11);
  double total = 0;
  size_t j = 0;
  for (size_t i = 0; i < values.size(); ++i) {
    total += values_sorted[i];

    if (std::binary_search(indices.begin(), indices.end(), i)) {
      model[j] = total / sum;
      ++j;
    }
  }

  std::vector<double> cs = CumulativeSumFractions(&values, 10);
  AssertAlmostEqual(model, cs);
}

TEST(Distribution, RandomValueTenPercentiles) {
  std::vector<double> values;
  for (size_t i = 0; i < kMillion; ++i) {
    values.emplace_back(std::rand() / static_cast<double>(RAND_MAX));
  }

  Distribution<double> distribution(&values, 10);

  std::vector<double> percentiles = Percentiles(&values, 10);
  std::vector<double> cs = CumulativeSumFractions(&values, 10);
  ASSERT_EQ(percentiles, distribution.quantiles());
  ASSERT_EQ(cs, distribution.cumulative_fractions());
  ASSERT_EQ(10, distribution.top_n().size());
}

TEST(SummaryStats, NoElements) {
  SummaryStats summary_stats;
  ASSERT_EQ(0, summary_stats.count());
  ASSERT_DEATH(summary_stats.mean(), ".*");
  ASSERT_DEATH(summary_stats.std(), ".*");
  ASSERT_DEATH(summary_stats.var(), ".*");
}

TEST(SummaryStats, SingleElement) {
  SummaryStats summary_stats;
  summary_stats.Add(1.0);

  ASSERT_EQ(1, summary_stats.count());
  ASSERT_EQ(1, summary_stats.mean());
  ASSERT_EQ(0, summary_stats.std());
  ASSERT_EQ(0, summary_stats.var());
}

TEST(SummaryStats, Overflow) {
  double very_large_number = std::pow(std::numeric_limits<double>::max(), 0.5);
  SummaryStats summary_stats;
  ASSERT_DEATH(summary_stats.Add(very_large_number), ".*");
}

TEST(ExpDetect, EmptySequence) {
  ASSERT_FALSE(ExpDetect({}, 2, 0, 1));
  ASSERT_TRUE(ExpDetect({}, 2, 0, 0));
  ASSERT_TRUE(ExpDetect({}, 0, 0, 0));
}

TEST(ExpDetect, SingleValue) {
  ASSERT_TRUE(ExpDetect({1}, 2, 0, 1));
  ASSERT_TRUE(ExpDetect({1}, 2, 0, 0));
  ASSERT_TRUE(ExpDetect({5}, 0, 0, 0));
}

TEST(ExpDetect, SequencesNoTolerance) {
  ASSERT_TRUE(ExpDetect({1, 2, 3, 4, 5}, 2, 0, 0));
  ASSERT_TRUE(ExpDetect({1, 2, 3, 4, 5}, 2, 0, 1));
  ASSERT_TRUE(ExpDetect({1, 2, 3, 4, 5}, 2, 0, 2));
  ASSERT_FALSE(ExpDetect({1, 2, 3, 4, 5}, 2, 0, 3));
  ASSERT_TRUE(ExpDetect({1, 2, 4, 5}, 2, 0, 3));
  ASSERT_TRUE(ExpDetect({1, 2, 4}, 2, 0, 3));
  ASSERT_TRUE(ExpDetect({0, 99, 1, 2, 4, 5}, 2, 0, 3));
  ASSERT_FALSE(ExpDetect({1, 2, 2, 3, 4, 5}, 2, 0, 3));
  ASSERT_TRUE(ExpDetect({5, 6, 5, 5, 1, 2, 4}, 2, 0, 3));
  ASSERT_TRUE(ExpDetect({1, 2, 1, 2, 4}, 2, 0, 3));
  ASSERT_TRUE(ExpDetect({1, 2, 4, 1, 2, 4}, 2, 0, 3));
}

TEST(ExpDetect, SequencesTolerance) {
  ASSERT_TRUE(ExpDetect({1, 2, 5, 1, 2, 5}, 2, 1, 3));
  ASSERT_TRUE(ExpDetect({1, 2, 4, 1, 2, 5}, 2, 1, 3));
  ASSERT_TRUE(ExpDetect({1, 2, 3, 1, 2, 5}, 2, 1, 3));
}

TEST(ExpDetect, SequencesPower) {
  ASSERT_TRUE(ExpDetect({4, 2, 1, 5, 12.5, 31.25, 78.125, 86}, 2.5, 0.001, 4));
}

TEST(ThresholdEnforcer, DefaultPolicyDefaultMissingValue) {
  ThresholdEnforcerPolicy enforcer_policy;
  ThresholdEnforcer<int> threshold_enforcer(enforcer_policy);

  // The default policy should enforce no thresholds.
  ASSERT_EQ(0.0, threshold_enforcer.Get(1));

  ASSERT_TRUE(threshold_enforcer.Change(1, 0.0));
  ASSERT_EQ(0.0, threshold_enforcer.Get(1));

  ASSERT_TRUE(threshold_enforcer.Change(1, 0.0001));
  ASSERT_EQ(0.0001, threshold_enforcer.Get(1));
}

TEST(ThresholdEnforcer, BadValue) {
  ThresholdEnforcerPolicy enforcer_policy;
  enforcer_policy.set_empty_threshold_absolute(0.0);
  enforcer_policy.set_empty_threshold_absolute(1.0);
  ASSERT_DEATH(enforcer_policy.set_empty_threshold_absolute(-1.0), ".*");

  enforcer_policy.set_threshold_absolute(0.0);
  enforcer_policy.set_threshold_absolute(1.0);
  ASSERT_DEATH(enforcer_policy.set_threshold_absolute(-1.0), ".*");

  enforcer_policy.set_threshold_relative_to_total(1.0);
  enforcer_policy.set_threshold_relative_to_total(0.0);
  ASSERT_DEATH(enforcer_policy.set_threshold_relative_to_total(1.5), ".*");
  ASSERT_DEATH(enforcer_policy.set_threshold_relative_to_total(-1.5), ".*");

  enforcer_policy.set_threshold_relative_to_current(1.0);
  enforcer_policy.set_threshold_relative_to_current(0.5);
  ASSERT_DEATH(enforcer_policy.set_threshold_relative_to_current(1.5), ".*");
  ASSERT_DEATH(enforcer_policy.set_threshold_relative_to_current(-1.5), ".*");
}

TEST(ThresholdEnforcer, AbsoluteEmptyThreshold) {
  ThresholdEnforcerPolicy enforcer_policy;
  enforcer_policy.set_empty_threshold_absolute(1.0);
  ThresholdEnforcer<int> threshold_enforcer(enforcer_policy);

  ASSERT_FALSE(threshold_enforcer.Change(1, 0.0));
  ASSERT_FALSE(threshold_enforcer.Change(1, 0.5));
  ASSERT_EQ(0.0, threshold_enforcer.Get(1));

  ASSERT_FALSE(threshold_enforcer.Change(2, 0.99));
  ASSERT_EQ(0.0, threshold_enforcer.Get(2));

  ASSERT_TRUE(threshold_enforcer.Change(2, -1.0));
  ASSERT_EQ(-1.0, threshold_enforcer.Get(2));

  ASSERT_TRUE(threshold_enforcer.Change(2, -1.1));
  ASSERT_EQ(-1.1, threshold_enforcer.Get(2));

  ASSERT_TRUE(threshold_enforcer.Change(2, 1.0));
  ASSERT_EQ(1.0, threshold_enforcer.Get(2));

  ASSERT_TRUE(threshold_enforcer.Change(2, 1.1));
  ASSERT_EQ(1.1, threshold_enforcer.Get(2));
}

TEST(ThresholdEnforcer, AbsoluteThreshold) {
  ThresholdEnforcerPolicy enforcer_policy;
  enforcer_policy.set_threshold_absolute(1.0);
  enforcer_policy.set_empty_threshold_absolute(10.0);
  ThresholdEnforcer<int> threshold_enforcer(enforcer_policy);

  ASSERT_FALSE(threshold_enforcer.Change(1, 0.0));
  ASSERT_EQ(0.0, threshold_enforcer.Get(1));

  ASSERT_FALSE(threshold_enforcer.Change(1, 0.5));
  ASSERT_EQ(0.0, threshold_enforcer.Get(1));

  ASSERT_FALSE(threshold_enforcer.Change(1, 0.99));
  ASSERT_EQ(0.0, threshold_enforcer.Get(1));

  ASSERT_FALSE(threshold_enforcer.Change(5, 0.99));
  ASSERT_EQ(0.0, threshold_enforcer.Get(1));

  ASSERT_TRUE(threshold_enforcer.Change(2, 10.0));
  ASSERT_EQ(10.0, threshold_enforcer.Get(2));

  ASSERT_FALSE(threshold_enforcer.Change(2, 10.5));
  ASSERT_EQ(10.0, threshold_enforcer.Get(2));

  ASSERT_FALSE(threshold_enforcer.Change(2, 9.5));
  ASSERT_EQ(10.0, threshold_enforcer.Get(2));

  ASSERT_TRUE(threshold_enforcer.Change(2, 11.5));
  ASSERT_EQ(11.5, threshold_enforcer.Get(2));
}

TEST(ThresholdEnforcer, RelativeFromTotal) {
  ThresholdEnforcerPolicy enforcer_policy;
  enforcer_policy.set_threshold_relative_to_total(0.1);
  ThresholdEnforcer<int> threshold_enforcer(enforcer_policy);

  ASSERT_TRUE(threshold_enforcer.Change(1, 5.0));
  ASSERT_TRUE(threshold_enforcer.Change(2, 5.0));
  ASSERT_TRUE(threshold_enforcer.Change(3, 20.0));

  // Too low -- 10% of 30 is 3.
  ASSERT_FALSE(threshold_enforcer.Change(4, 2.0));

  ASSERT_TRUE(threshold_enforcer.Change(3, 3.0));

  // Should be fine now.
  ASSERT_TRUE(threshold_enforcer.Change(4, 2.0));
}

TEST(ThresholdEnforcer, BulkChange) {
  ThresholdEnforcerPolicy enforcer_policy;
  enforcer_policy.set_threshold_absolute(1.0);
  ThresholdEnforcer<int> threshold_enforcer(enforcer_policy);

  ASSERT_FALSE(
      threshold_enforcer.ChangeBulk({{1, 0.1}, {2, 0.2}, {3, 0.9}, {4, -0.5}}));
  ASSERT_TRUE(
      threshold_enforcer.ChangeBulk({{1, 0.1}, {2, 0.2}, {3, 1.0}, {4, -0.5}}));
  ASSERT_EQ(0.1, threshold_enforcer.Get(1));

  ASSERT_FALSE(threshold_enforcer.ChangeBulk({{1, 0.1}, {3, 1.0}, {4, -0.5}}));
  ASSERT_TRUE(threshold_enforcer.ChangeBulk({{1, 0.1}, {4, -0.5}}));
  ASSERT_EQ(0.0, threshold_enforcer.Get(2));
  ASSERT_EQ(0.0, threshold_enforcer.Get(3));
}

TEST(TimeoutEnforcer, DefaultPolicy) {
  TimeoutPolicy timeout_policy;
  TimeoutEnforcer<int> timeout_enforcer(timeout_policy);
  timeout_enforcer.Update(1, 10);
  timeout_enforcer.Update(2, 20);

  // Time has advanced past 10.
  ASSERT_DEATH(timeout_enforcer.Timeout(10), ".*");

  // By default the enforcer will immediately time values out.
  ASSERT_EQ(std::vector<int>({1, 2}), timeout_enforcer.Timeout(20));
}

TEST(TimeoutEnforcer, BadUpdate) {
  TimeoutPolicy timeout_policy;
  TimeoutEnforcer<int> timeout_enforcer(timeout_policy);
  timeout_enforcer.Update(1, 10);
  timeout_enforcer.Update(1, 10);
  ASSERT_DEATH(timeout_enforcer.Update(1, 9), ".*");
}

TEST(TimeoutEnforcer, SingleKey) {
  TimeoutPolicy timeout_policy;
  timeout_policy.set_base_timeout(100);

  TimeoutEnforcer<int> timeout_enforcer(timeout_policy);
  timeout_enforcer.Update(1, 10);
  ASSERT_EQ(std::vector<int>({}), timeout_enforcer.Timeout(10));
  ASSERT_EQ(std::vector<int>({}), timeout_enforcer.Timeout(100));
  ASSERT_EQ(std::vector<int>({1}), timeout_enforcer.Timeout(110));
  ASSERT_EQ(std::vector<int>({}), timeout_enforcer.Timeout(500));

  timeout_enforcer.Update(2, 10);
  timeout_enforcer.Update(2, 50);
  timeout_enforcer.Update(2, 100);
  ASSERT_EQ(std::vector<int>({}), timeout_enforcer.Timeout(190));
  ASSERT_EQ(std::vector<int>({2}), timeout_enforcer.Timeout(250));
}

TEST(TimeoutEnforcer, SingleKeyMultiRemove) {
  TimeoutPolicy timeout_policy;
  timeout_policy.set_base_timeout(100);

  TimeoutEnforcer<int> timeout_enforcer(timeout_policy);
  timeout_enforcer.Update(1, 10);
  ASSERT_EQ(std::vector<int>({1}), timeout_enforcer.Timeout(200));
  timeout_enforcer.Update(1, 210);
  ASSERT_EQ(std::vector<int>({1}), timeout_enforcer.Timeout(400));
}

TEST(TimeoutEnforcer, SingleKeyPenalty) {
  TimeoutPolicy timeout_policy;
  timeout_policy.set_base_timeout(100);
  timeout_policy.set_timeout_penalty(100);
  timeout_policy.set_timeout_penalty_lookback(500);

  TimeoutEnforcer<int> timeout_enforcer(timeout_policy);
  timeout_enforcer.Update(1, 10);
  ASSERT_EQ(std::vector<int>({1}), timeout_enforcer.Timeout(150));

  // The update for the key (200) is within 500 of the last update (10), so the
  // penalty should kick in.
  timeout_enforcer.Update(1, 200);
  ASSERT_EQ(std::vector<int>({}), timeout_enforcer.Timeout(350));

  // 400 is 100 (base) + 100 (penalty) from the last update (200).
  ASSERT_EQ(std::vector<int>({1}), timeout_enforcer.Timeout(400));
}

TEST(TimeoutEnforcer, SingleKeyPenaltyCumulative) {
  TimeoutPolicy timeout_policy;
  timeout_policy.set_base_timeout(100);
  timeout_policy.set_timeout_penalty(100);
  timeout_policy.set_timeout_penalty_lookback(500);
  timeout_policy.set_timeout_penalty_cumulative(true);

  TimeoutEnforcer<int> timeout_enforcer(timeout_policy);
  timeout_enforcer.Update(1, 10);
  ASSERT_EQ(std::vector<int>({1}), timeout_enforcer.Timeout(150));

  // The update for the key (200) is within 500 of the last update (10), so the
  // penalty should kick in.
  timeout_enforcer.Update(1, 200);
  ASSERT_EQ(std::vector<int>({}), timeout_enforcer.Timeout(350));

  // Another update within the penalty lookback.
  timeout_enforcer.Update(1, 400);

  // 100 away from last update, but 2x penalty.
  ASSERT_EQ(std::vector<int>({}), timeout_enforcer.Timeout(500));

  // 700 is 100 (base) + 200 (penalty) from the last update (400).
  ASSERT_EQ(std::vector<int>({1}), timeout_enforcer.Timeout(700));
}

}  // namespace
}  // namespace ncode
