#include "predict.h"

namespace ncode {

double Predictor::Predict() {
  CHECK(!values_.empty());
  return PredictNext(values_.size() - 1);
}

std::vector<double> Predictor::GetErrors() {
  std::vector<double> errors;
  for (size_t i = 0; i < values_.size() - 1; ++i) {
    double prediction = PredictNext(i);
    double true_value = values_[i + 1];
    double error = (true_value - prediction) / true_value;
    errors.emplace_back(error);
  }

  return errors;
}

}  // namespace ncode
