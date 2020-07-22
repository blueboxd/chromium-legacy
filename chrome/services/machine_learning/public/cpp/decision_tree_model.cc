// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/machine_learning/public/cpp/decision_tree_model.h"

#include <memory>
#include <string>
#include <utility>

#include "chrome/services/machine_learning/public/cpp/decision_tree/prediction_model.h"
#include "chrome/services/machine_learning/public/mojom/decision_tree.mojom.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace machine_learning {

DecisionTreeModel::DecisionTreeModel(
    std::unique_ptr<optimization_guide::proto::PredictionModel> model_proto)
    : prediction_model_(model_proto ? decision_tree::PredictionModel::Create(
                                          std::move(model_proto))
                                    : nullptr) {}

DecisionTreeModel::~DecisionTreeModel() = default;

// static
std::unique_ptr<DecisionTreeModel> DecisionTreeModel::FromModelSpec(
    mojom::DecisionTreeModelSpecPtr spec) {
  if (!spec)
    return nullptr;
  auto model_proto =
      std::make_unique<optimization_guide::proto::PredictionModel>();

  if (model_proto->ParseFromString(std::move(spec)->serialized_model))
    return std::make_unique<DecisionTreeModel>(std::move(model_proto));

  return nullptr;
}

mojom::DecisionTreePredictionResult DecisionTreeModel::Predict(
    const base::flat_map<std::string, float>& model_features,
    double* prediction_score) {
  if (!IsValid())
    return mojom::DecisionTreePredictionResult::kUnknown;

  double score = 0.0;
  mojom::DecisionTreePredictionResult result =
      prediction_model_->Predict(model_features, &score);

  if (prediction_score)
    *prediction_score = score;

  return result;
}

bool DecisionTreeModel::IsValid() const {
  return prediction_model_ != nullptr;
}

}  // namespace machine_learning
