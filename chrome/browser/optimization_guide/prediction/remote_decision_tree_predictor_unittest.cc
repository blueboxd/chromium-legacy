// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/prediction/remote_decision_tree_predictor.h"

#include <memory>

#include "base/test/task_environment.h"
#include "components/optimization_guide/optimization_guide_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

const double kThreshold = 1.0;
const double kWeight = 1.0;
const double kModelValueDiff = 0.2;
const int64_t kVersion = 1;

std::unique_ptr<proto::PredictionModel> GetValidPredictionModel() {
  // This model will return true upon evaluation.
  auto model = GetSingleLeafDecisionTreePredictionModel(
      kThreshold, kWeight, (kThreshold + kModelValueDiff) / kWeight);

  model->mutable_model_info()->set_version(kVersion);
  model->mutable_model_info()->add_supported_model_types(
      optimization_guide::proto::MODEL_TYPE_DECISION_TREE);
  model->mutable_model_info()->set_optimization_target(
      proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);

  return model;
}

}  // namespace

TEST(RemoteDecisionTreePredictorTest, Initialization) {
  base::test::SingleThreadTaskEnvironment task_environment;

  auto model = GetValidPredictionModel();
  RemoteDecisionTreePredictor predictor(*model);

  EXPECT_EQ(kVersion, predictor.version());
  EXPECT_TRUE(predictor.model_features().empty());
  EXPECT_FALSE(predictor.Get());
}

TEST(RemoteDecisionTreePredictorTest, BindPredictorToReceiver) {
  base::test::SingleThreadTaskEnvironment task_environment;

  auto model = GetValidPredictionModel();
  RemoteDecisionTreePredictor predictor(*model);

  auto pending_receiver = predictor.BindNewPipeAndPassReceiver();
  EXPECT_TRUE(predictor.Get());
  EXPECT_TRUE(pending_receiver);
  EXPECT_TRUE(predictor.IsConnected());

  pending_receiver.reset();
  predictor.FlushForTesting();
  EXPECT_TRUE(predictor.Get());
  EXPECT_FALSE(predictor.IsConnected());
}

}  // namespace optimization_guide
