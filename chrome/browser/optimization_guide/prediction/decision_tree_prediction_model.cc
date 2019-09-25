// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/prediction/decision_tree_prediction_model.h"
#include "chrome/browser/optimization_guide/prediction/prediction_model.h"

namespace optimization_guide {

DecisionTreePredictionModel::DecisionTreePredictionModel(
    std::unique_ptr<optimization_guide::proto::PredictionModel>
        prediction_model,
    const base::flat_set<std::string>& host_model_features)
    : PredictionModel(std::move(prediction_model), host_model_features) {}

DecisionTreePredictionModel::~DecisionTreePredictionModel() = default;

optimization_guide::OptimizationTargetDecision
DecisionTreePredictionModel::Predict(
    const base::flat_map<std::string, float>& model_features) {
  SEQUENCE_CHECKER(sequence_checker_);
  // TODO(crbug/1001194): Add decision tree evaluation implementation.
  return optimization_guide::OptimizationTargetDecision::kUnknown;
}

}  // namespace optimization_guide
