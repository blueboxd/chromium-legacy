// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "chrome/browser/optimization_guide/prediction/decision_tree_prediction_model.h"
#include "chrome/browser/optimization_guide/prediction/prediction_model.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

TEST(DecisionTreePredictionModel, ValidDecisionTreeModel) {
  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      std::make_unique<optimization_guide::proto::PredictionModel>();

  optimization_guide::proto::DecisionTree* decision_tree_model =
      prediction_model->mutable_model()->mutable_decision_tree();
  decision_tree_model->set_weight(2.0);

  optimization_guide::proto::ModelInfo* model_info =
      prediction_model->mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_types(
      optimization_guide::proto::ModelType::MODEL_TYPE_DECISION_TREE);
  model_info->add_supported_model_features(
      optimization_guide::proto::ClientModelFeature::
          CLIENT_MODEL_FEATURE_EFFECTIVE_CONNECTION_TYPE);

  std::unique_ptr<PredictionModel> model =
      PredictionModel::Create(std::move(prediction_model), {"agg1"});
  base::flat_map<std::string, float> feature_map = {};
  EXPECT_EQ(1, model->GetVersion());
  EXPECT_EQ(2u, model->GetModelFeatures().size());
  EXPECT_TRUE(model->GetModelFeatures().count("agg1"));
  EXPECT_TRUE(model->GetModelFeatures().count(
      "CLIENT_MODEL_FEATURE_EFFECTIVE_CONNECTION_TYPE"));
  EXPECT_EQ(optimization_guide::OptimizationTargetDecision::kUnknown,
            model->Predict(feature_map));
}

}  // namespace optimization_guide
