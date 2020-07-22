// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "chrome/services/machine_learning/public/cpp/decision_tree/decision_tree_prediction_model.h"
#include "chrome/services/machine_learning/public/cpp/decision_tree/prediction_model.h"
#include "chrome/services/machine_learning/public/mojom/decision_tree.mojom.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace machine_learning {
namespace decision_tree {

using machine_learning::decision_tree::PredictionModel;

std::unique_ptr<optimization_guide::proto::PredictionModel>
GetValidDecisionTreePredictionModel() {
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
  return prediction_model;
}

std::unique_ptr<optimization_guide::proto::PredictionModel>
GetValidEnsemblePredictionModel() {
  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      std::make_unique<optimization_guide::proto::PredictionModel>();
  prediction_model->mutable_model()->mutable_threshold()->set_value(5.0);
  optimization_guide::proto::Ensemble ensemble =
      optimization_guide::proto::Ensemble();
  *ensemble.add_members()->mutable_submodel() =
      *GetValidDecisionTreePredictionModel()->mutable_model();

  *ensemble.add_members()->mutable_submodel() =
      *GetValidDecisionTreePredictionModel()->mutable_model();

  *prediction_model->mutable_model()->mutable_ensemble() = ensemble;
  return prediction_model;
}

TEST(DecisionTreePredictionModel, ValidDecisionTreeModel) {
  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      GetValidDecisionTreePredictionModel();

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
  EXPECT_TRUE(model);

  double prediction_score;
  EXPECT_EQ(mojom::DecisionTreePredictionResult::kFalse,
            model->Predict({{"agg1", 1.0}}, &prediction_score));
  EXPECT_EQ(4., prediction_score);
  EXPECT_EQ(mojom::DecisionTreePredictionResult::kTrue,
            model->Predict({{"agg1", 2.0}}, &prediction_score));
  EXPECT_EQ(8., prediction_score);
}

TEST(DecisionTreePredictionModel, InequalityLessThan) {
  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      GetValidDecisionTreePredictionModel();

  prediction_model->mutable_model()
      ->mutable_decision_tree()
      ->mutable_nodes(0)
      ->mutable_binary_node()
      ->mutable_inequality_left_child_test()
      ->set_type(optimization_guide::proto::InequalityTest::LESS_THAN);

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
  EXPECT_TRUE(model);

  double prediction_score;
  EXPECT_EQ(mojom::DecisionTreePredictionResult::kFalse,
            model->Predict({{"agg1", 0.5}}, &prediction_score));
  EXPECT_EQ(4., prediction_score);
  EXPECT_EQ(mojom::DecisionTreePredictionResult::kTrue,
            model->Predict({{"agg1", 2.0}}, &prediction_score));
  EXPECT_EQ(8., prediction_score);
}

TEST(DecisionTreePredictionModel, InequalityGreaterOrEqual) {
  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      GetValidDecisionTreePredictionModel();

  prediction_model->mutable_model()
      ->mutable_decision_tree()
      ->mutable_nodes(0)
      ->mutable_binary_node()
      ->mutable_inequality_left_child_test()
      ->set_type(optimization_guide::proto::InequalityTest::GREATER_OR_EQUAL);

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
  EXPECT_TRUE(model);

  double prediction_score;
  EXPECT_EQ(mojom::DecisionTreePredictionResult::kTrue,
            model->Predict({{"agg1", 0.5}}, &prediction_score));
  EXPECT_EQ(8., prediction_score);
  EXPECT_EQ(mojom::DecisionTreePredictionResult::kFalse,
            model->Predict({{"agg1", 1.0}}, &prediction_score));
  EXPECT_EQ(4., prediction_score);
}

TEST(DecisionTreePredictionModel, InequalityGreaterThan) {
  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      GetValidDecisionTreePredictionModel();

  prediction_model->mutable_model()
      ->mutable_decision_tree()
      ->mutable_nodes(0)
      ->mutable_binary_node()
      ->mutable_inequality_left_child_test()
      ->set_type(optimization_guide::proto::InequalityTest::GREATER_THAN);

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
  EXPECT_TRUE(model);

  double prediction_score;
  EXPECT_EQ(mojom::DecisionTreePredictionResult::kTrue,
            model->Predict({{"agg1", 0.5}}, &prediction_score));
  EXPECT_EQ(8., prediction_score);
  EXPECT_EQ(mojom::DecisionTreePredictionResult::kFalse,
            model->Predict({{"agg1", 2.0}}, &prediction_score));
  EXPECT_EQ(4., prediction_score);
}

TEST(DecisionTreePredictionModel, MissingInequalityTest) {
  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      GetValidDecisionTreePredictionModel();

  prediction_model->mutable_model()
      ->mutable_decision_tree()
      ->mutable_nodes(0)
      ->mutable_binary_node()
      ->mutable_inequality_left_child_test()
      ->Clear();

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
  EXPECT_FALSE(model);
}

TEST(DecisionTreePredictionModel, NoDecisionTreeThreshold) {
  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      GetValidDecisionTreePredictionModel();

  prediction_model->mutable_model()->clear_threshold();

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
  EXPECT_FALSE(model);
}

TEST(DecisionTreePredictionModel, EmptyTree) {
  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      GetValidDecisionTreePredictionModel();

  prediction_model->mutable_model()->mutable_decision_tree()->clear_nodes();

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
  EXPECT_FALSE(model);
}

TEST(DecisionTreePredictionModel, ModelFeatureNotInFeatureMap) {
  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      GetValidDecisionTreePredictionModel();

  prediction_model->mutable_model()->mutable_decision_tree()->clear_nodes();

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
  EXPECT_FALSE(model);
}

TEST(DecisionTreePredictionModel, DecisionTreeMissingLeaf) {
  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      GetValidDecisionTreePredictionModel();

  prediction_model->mutable_model()
      ->mutable_decision_tree()
      ->mutable_nodes(1)
      ->mutable_leaf()
      ->Clear();

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
  EXPECT_FALSE(model);
}

TEST(DecisionTreePredictionModel, DecisionTreeLeftChildIndexInvalid) {
  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      GetValidDecisionTreePredictionModel();

  prediction_model->mutable_model()
      ->mutable_decision_tree()
      ->mutable_nodes(0)
      ->mutable_binary_node()
      ->mutable_left_child_id()
      ->set_value(3);

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
  EXPECT_FALSE(model);
}

TEST(DecisionTreePredictionModel, DecisionTreeRightChildIndexInvalid) {
  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      GetValidDecisionTreePredictionModel();

  prediction_model->mutable_model()
      ->mutable_decision_tree()
      ->mutable_nodes(0)
      ->mutable_binary_node()
      ->mutable_right_child_id()
      ->set_value(3);

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
  EXPECT_FALSE(model);
}

TEST(DecisionTreePredictionModel, DecisionTreeWithLoopOnLeftChild) {
  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      GetValidDecisionTreePredictionModel();

  optimization_guide::proto::TreeNode* tree_node =
      prediction_model->mutable_model()->mutable_decision_tree()->mutable_nodes(
          1);

  tree_node->mutable_node_id()->set_value(0);
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

  tree_node->mutable_binary_node()->mutable_left_child_id()->set_value(0);
  tree_node->mutable_binary_node()->mutable_right_child_id()->set_value(2);

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
  EXPECT_FALSE(model);
}

TEST(DecisionTreePredictionModel, DecisionTreeWithLoopOnRightChild) {
  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      GetValidDecisionTreePredictionModel();

  optimization_guide::proto::TreeNode* tree_node =
      prediction_model->mutable_model()->mutable_decision_tree()->mutable_nodes(
          1);

  tree_node->mutable_node_id()->set_value(0);
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

  tree_node->mutable_binary_node()->mutable_left_child_id()->set_value(2);
  tree_node->mutable_binary_node()->mutable_right_child_id()->set_value(0);

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
  EXPECT_FALSE(model);
}

TEST(DecisionTreePredictionModel, ValidEnsembleModel) {
  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      GetValidEnsemblePredictionModel();

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
  EXPECT_TRUE(model);

  double prediction_score;
  EXPECT_EQ(mojom::DecisionTreePredictionResult::kFalse,
            model->Predict({{"agg1", 1.0}}, &prediction_score));
  EXPECT_EQ(4., prediction_score);
  EXPECT_EQ(mojom::DecisionTreePredictionResult::kTrue,
            model->Predict({{"agg1", 2.0}}, &prediction_score));
  EXPECT_EQ(8., prediction_score);
}

TEST(DecisionTreePredictionModel, EnsembleWithNoMembers) {
  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      GetValidEnsemblePredictionModel();
  prediction_model->mutable_model()
      ->mutable_ensemble()
      ->mutable_members()
      ->Clear();

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
  EXPECT_FALSE(model);
}

}  // namespace decision_tree
}  // namespace machine_learning
