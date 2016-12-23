#ifndef NCODE_PREDICT_H
#define NCODE_PREDICT_H

#include <vector>
#include "common.h"

namespace ncode {

// Given a series of values will predict the next one. Also keeps track of
// prediction error.
class Predictor {
 public:
  virtual ~Predictor() {}

  // Predicts the next value based on previous values.
  double Predict();

  // Adds a new value.
  virtual void Add(double value) { values_.emplace_back(value); }

  // Returns for each value the error between its predicted value and real
  // value. The returned vector will have N-1 elements if there are N values.
  std::vector<double> GetErrors();

 protected:
  // Predicts the next value using values up to (and including) 'index_to'.
  virtual double PredictNext(size_t index_to) = 0;

  std::vector<double> values_;
};

// A simple predictor that always predicts the previous value.
class DummyPredictor : public Predictor {
 protected:
  double PredictNext(size_t index_to) override { return values_[index_to]; }
};
}  // namespace ncode

#endif
