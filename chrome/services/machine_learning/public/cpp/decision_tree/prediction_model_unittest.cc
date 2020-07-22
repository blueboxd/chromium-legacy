// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/machine_learning/public/cpp/decision_tree/prediction_model.h"

#include <utility>

#include "components/optimization_guide/proto/models.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace machine_learning {
namespace decision_tree {

TEST(PredictionModelTest, ValidPredictionModel) {
  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      std::make_unique<optimization_guide::proto::PredictionModel>();
  prediction_model->mutable_model()->mutable_threshold()->set_value(5.0);

  optimization_guide::proto::DecisionTree decision_tree_model =
      optimization_guide::proto::DecisionTree();
  decision_tree_model.set_weight(2.0);

  optimization_guide::proto::TreeNode* tree_node =
      decision_tree_model.add_nodes();
  tree_node->mutable_node_id()->set_value(0);
  tree_node->mutable_binary_node()->mutable_left_child_id()->set_value(1);
  tree_node->mutable_binary_node()->mutable_right_child_id()->set_value(2);
  tree_node->mutable_binary_node()
      ->mutable_inequality_left_child_test()
      ->mutable_feature_id()
      ->mutable_id()
      ->set_value("agg1");
  tree_node->mutable_binary_node()
      ->mutable_inequality_left_child_test()
      ->set_type(optimization_guide::proto::InequalityTest::LESS_OR_EQUAL);
  tree_node->mutable_binary_node()
      ->mutable_inequality_left_child_test()
      ->mutable_threshold()
      ->set_float_value(1.0);

  tree_node = decision_tree_model.add_nodes();
  tree_node->mutable_node_id()->set_value(1);
  tree_node->mutable_leaf()->mutable_vector()->add_value()->set_double_value(
      2.);

  tree_node = decision_tree_model.add_nodes();
  tree_node->mutable_node_id()->set_value(2);
  tree_node->mutable_leaf()->mutable_vector()->add_value()->set_double_value(
      4.);

  *prediction_model->mutable_model()->mutable_decision_tree() =
      decision_tree_model;

  optimization_guide::proto::ModelInfo* model_info =
      prediction_model->mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_types(
      optimization_guide::proto::ModelType::MODEL_TYPE_DECISION_TREE);
  model_info->add_supported_model_features(
      optimization_guide::proto::ClientModelFeature::
          CLIENT_MODEL_FEATURE_EFFECTIVE_CONNECTION_TYPE);
  model_info->add_supported_host_model_features("agg1");

  std::unique_ptr<PredictionModel> model =
      PredictionModel::Create(std::move(prediction_model));

  EXPECT_EQ(1, model->GetVersion());
  EXPECT_EQ(2u, model->GetModelFeatures().size());
  EXPECT_TRUE(model->GetModelFeatures().count("agg1"));
  EXPECT_TRUE(model->GetModelFeatures().count(
      "CLIENT_MODEL_FEATURE_EFFECTIVE_CONNECTION_TYPE"));
}

TEST(PredictionModelTest, NoModel) {
  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      std::make_unique<optimization_guide::proto::PredictionModel>();

  EXPECT_FALSE(PredictionModel::Create(std::move(prediction_model)));
}

TEST(PredictionModelTest, NoModelVersion) {
  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      std::make_unique<optimization_guide::proto::PredictionModel>();

  optimization_guide::proto::DecisionTree* decision_tree_model =
      prediction_model->mutable_model()->mutable_decision_tree();
  decision_tree_model->set_weight(2.0);

  EXPECT_FALSE(PredictionModel::Create(std::move(prediction_model)));
}

TEST(PredictionModelTest, NoModelType) {
  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      std::make_unique<optimization_guide::proto::PredictionModel>();

  optimization_guide::proto::DecisionTree* decision_tree_model =
      prediction_model->mutable_model()->mutable_decision_tree();
  decision_tree_model->set_weight(2.0);

  optimization_guide::proto::ModelInfo* model_info =
      prediction_model->mutable_model_info();
  model_info->set_version(1);

  EXPECT_FALSE(PredictionModel::Create(std::move(prediction_model)));
}

TEST(PredictionModelTest, UnknownModelType) {
  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      std::make_unique<optimization_guide::proto::PredictionModel>();

  optimization_guide::proto::DecisionTree* decision_tree_model =
      prediction_model->mutable_model()->mutable_decision_tree();
  decision_tree_model->set_weight(2.0);

  optimization_guide::proto::ModelInfo* model_info =
      prediction_model->mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_types(
      optimization_guide::proto::ModelType::MODEL_TYPE_UNKNOWN);

  EXPECT_FALSE(PredictionModel::Create(std::move(prediction_model)));
}

TEST(PredictionModelTest, MultipleModelTypes) {
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
  model_info->add_supported_model_types(
      optimization_guide::proto::ModelType::MODEL_TYPE_UNKNOWN);

  EXPECT_FALSE(PredictionModel::Create(std::move(prediction_model)));
}

TEST(PredictionModelTest, UnknownModelClientFeature) {
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
          CLIENT_MODEL_FEATURE_UNKNOWN);

  EXPECT_FALSE(PredictionModel::Create(std::move(prediction_model)));
}

}  // namespace decision_tree
}  // namespace machine_learning
