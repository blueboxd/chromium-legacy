// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/prediction/prediction_manager.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/optimization_guide/prediction/prediction_model.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

namespace optimization_guide {

std::unique_ptr<proto::PredictionModel> CreateTestPredictionModel() {
  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      std::make_unique<optimization_guide::proto::PredictionModel>();

  optimization_guide::proto::ModelInfo* model_info =
      prediction_model->mutable_model_info();
  model_info->set_version(1);
  model_info->add_supported_model_features(
      proto::CLIENT_MODEL_FEATURE_EFFECTIVE_CONNECTION_TYPE);
  return prediction_model;
}

class TestPredictionModel : public PredictionModel {
 public:
  TestPredictionModel(std::unique_ptr<proto::PredictionModel> prediction_model,
                      const base::flat_set<std::string>& host_model_features)
      : PredictionModel(std::move(prediction_model), host_model_features) {}
  ~TestPredictionModel() override = default;

  optimization_guide::OptimizationTargetDecision Predict(
      const base::flat_map<std::string, float>& model_features) override {
    // Check to make sure the all model_features were provided.
    for (const auto& model_feature : GetModelFeatures()) {
      if (!model_features.contains(model_feature))
        return OptimizationTargetDecision::kUnknown;
    }
    model_evaluated_ = true;
    return OptimizationTargetDecision::kPageLoadMatches;
  }

  bool WasModelEvaluated() { return model_evaluated_; }

  void ResetModelEvaluationState() { model_evaluated_ = false; }

 private:
  bool ValidatePredictionModel() const override { return true; }

  bool model_evaluated_ = false;
};

class TestPredictionManager : public PredictionManager {
 public:
  TestPredictionManager() = default;
  ~TestPredictionManager() override = default;

  void SetPredictionModel(proto::OptimizationTarget optimization_target,
                          std::unique_ptr<PredictionModel> prediction_model) {
    SeedPredictionModelForTesting(optimization_target,
                                  std::move(prediction_model));
  }

  void SetHostModelFeatures(
      const std::string& host,
      base::flat_map<std::string, float> host_model_features) {
    SeedHostModelFeaturesMapForTesting(host, host_model_features);
  }

  PredictionModel* GetPredictionModel(
      proto::OptimizationTarget optimization_target) const {
    return GetPredictionModelForTesting(optimization_target);
  }
};

class PredictionManagerTest
    : public ChromeRenderViewHostTestHarness,
      public testing::WithParamInterface<proto::ClientModelFeature> {
 public:
  PredictionManagerTest() = default;
  ~PredictionManagerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    prediction_manager_ = std::make_unique<TestPredictionManager>();
  }

  TestPredictionManager* prediction_manager() {
    return prediction_manager_.get();
  }

  bool IsSameOriginNavigationFeature() {
    return GetParam() == proto::CLIENT_MODEL_FEATURE_SAME_ORIGIN_NAVIGATION;
  }

  void TearDown() override { ChromeRenderViewHostTestHarness::TearDown(); }

 private:
  std::unique_ptr<TestPredictionManager> prediction_manager_;
  DISALLOW_COPY_AND_ASSIGN(PredictionManagerTest);
};

TEST_F(PredictionManagerTest, OptimizationTargetNotRegisteredForNavigation) {
  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_url(GURL("https://foo.com"));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});
  EXPECT_EQ(OptimizationTargetDecision::kUnknown,
            prediction_manager()->ShouldTargetNavigation(
                &navigation_handle, proto::OPTIMIZATION_TARGET_UNKNOWN));
}

TEST_F(PredictionManagerTest,
       NoPredictionModelForRegisteredOptimizationTarget) {
  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_url(GURL("https://foo.com"));

  prediction_manager()->RegisterOptimizationTargets(
      {proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});
  EXPECT_EQ(
      OptimizationTargetDecision::kModelNotAvailableOnClient,
      prediction_manager()->ShouldTargetNavigation(
          &navigation_handle, proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
}

TEST_F(PredictionManagerTest, EvaluatePredictionModel) {
  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_url(GURL("https://foo.com"));

  std::unique_ptr<TestPredictionModel> prediction_model =
      std::make_unique<TestPredictionModel>(CreateTestPredictionModel(),
                                            base::flat_set<std::string>());
  prediction_manager()->SetPredictionModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      std::move(prediction_model));

  EXPECT_EQ(
      OptimizationTargetDecision::kPageLoadMatches,
      prediction_manager()->ShouldTargetNavigation(
          &navigation_handle, proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));

  TestPredictionModel* test_prediction_model =
      static_cast<TestPredictionModel*>(
          prediction_manager()->GetPredictionModel(
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));
  EXPECT_TRUE(test_prediction_model);
  EXPECT_TRUE(test_prediction_model->WasModelEvaluated());
}

TEST_F(PredictionManagerTest, HasHostModelFeaturesForHost) {
  base::HistogramTester histogram_tester;
  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_url(GURL("https://foo.com"));

  std::unique_ptr<TestPredictionModel> prediction_model =
      std::make_unique<TestPredictionModel>(
          CreateTestPredictionModel(),
          base::flat_set<std::string>({{"host_feat1"}}));
  prediction_manager()->SetPredictionModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      std::move(prediction_model));

  prediction_manager()->SetHostModelFeatures(navigation_handle.GetURL().host(),
                                             {{"host_feat1", 1.0}});

  EXPECT_EQ(
      OptimizationTargetDecision::kPageLoadMatches,
      prediction_manager()->ShouldTargetNavigation(
          &navigation_handle, proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));

  TestPredictionModel* test_prediction_model =
      static_cast<TestPredictionModel*>(
          prediction_manager()->GetPredictionModel(
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));

  EXPECT_TRUE(test_prediction_model);
  EXPECT_TRUE(test_prediction_model->WasModelEvaluated());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager."
      "HasHostModelFeaturesForHost",
      true, 1);
}

TEST_F(PredictionManagerTest, NoHostModelFeaturesForHost) {
  base::HistogramTester histogram_tester;
  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_url(GURL("https://foo.com"));

  std::unique_ptr<TestPredictionModel> prediction_model =
      std::make_unique<TestPredictionModel>(
          CreateTestPredictionModel(),
          base::flat_set<std::string>({{"host_feat1"}}));
  prediction_manager()->SetPredictionModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      std::move(prediction_model));

  prediction_manager()->UpdateFCPSessionStatistics(
      base::TimeDelta::FromMilliseconds(1000.0));
  EXPECT_EQ(
      OptimizationTargetDecision::kPageLoadMatches,
      prediction_manager()->ShouldTargetNavigation(
          &navigation_handle, proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));

  TestPredictionModel* test_prediction_model =
      static_cast<TestPredictionModel*>(
          prediction_manager()->GetPredictionModel(
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));

  EXPECT_TRUE(test_prediction_model);
  EXPECT_TRUE(test_prediction_model->WasModelEvaluated());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager."
      "HasHostModelFeaturesForHost",
      false, 1);
}

TEST_P(PredictionManagerTest, ClientFeature) {
  base::HistogramTester histogram_tester;
  content::MockNavigationHandle navigation_handle(web_contents());
  GURL previous_url = GURL("https://foo.com");
  navigation_handle.set_url(previous_url);
  navigation_handle.set_page_transition(
      ui::PageTransition::PAGE_TRANSITION_RELOAD);
  ON_CALL(navigation_handle, GetPreviousURL())
      .WillByDefault(testing::ReturnRef(previous_url));

  if (IsSameOriginNavigationFeature()) {
    EXPECT_CALL(navigation_handle, GetPreviousURL()).Times(1);
  }

  std::unique_ptr<proto::PredictionModel> model = CreateTestPredictionModel();
  model->mutable_model_info()->clear_supported_model_features();
  model->mutable_model_info()->add_supported_model_features(GetParam());

  std::unique_ptr<TestPredictionModel> prediction_model =
      std::make_unique<TestPredictionModel>(std::move(model),
                                            base::flat_set<std::string>({{}}));

  prediction_manager()->SetPredictionModel(
      proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      std::move(prediction_model));

  EXPECT_EQ(
      OptimizationTargetDecision::kPageLoadMatches,
      prediction_manager()->ShouldTargetNavigation(
          &navigation_handle, proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));

  TestPredictionModel* test_prediction_model =
      static_cast<TestPredictionModel*>(
          prediction_manager()->GetPredictionModel(
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD));

  EXPECT_TRUE(test_prediction_model);
  EXPECT_TRUE(test_prediction_model->WasModelEvaluated());
}

INSTANTIATE_TEST_SUITE_P(ClientFeature,
                         PredictionManagerTest,
                         testing::Range(proto::ClientModelFeature_MIN,
                                        proto::ClientModelFeature_MAX));

}  // namespace optimization_guide
