#include "gtest/gtest.h"
#include "predict.h"

namespace ncode {
namespace {

class DummyPredictorTest : public ::testing::Test {
 protected:
  DummyPredictor predictor_;
};

TEST_F(DummyPredictorTest, Empty) { ASSERT_DEATH(predictor_.Predict(), ".*"); }

TEST_F(DummyPredictorTest, SingleValue) {
  predictor_.Add(10);
  ASSERT_EQ(10.0, predictor_.Predict());
}

TEST_F(DummyPredictorTest, MultipleValues) {
  predictor_.Add(10);
  predictor_.Add(11);
  predictor_.Add(-10);
  ASSERT_EQ(-10.0, predictor_.Predict());
}

TEST_F(DummyPredictorTest, SingleError) {
  predictor_.Add(10);

  std::vector<double> model;
  ASSERT_EQ(model, predictor_.GetErrors());
}

TEST_F(DummyPredictorTest, MultiError) {
  predictor_.Add(10);
  predictor_.Add(10);
  predictor_.Add(11);

  std::vector<double> model = {0.0, 1.0 / 11.0};
  ASSERT_EQ(model, predictor_.GetErrors());
}

}  // namespace
}  // namespace ncode
