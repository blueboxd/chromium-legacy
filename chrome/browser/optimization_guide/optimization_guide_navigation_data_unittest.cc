// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_navigation_data.h"

#include <memory>

#include "base/base64.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::AnyOf;
using testing::HasSubstr;
using testing::Not;

typedef struct {
  optimization_guide::proto::ClientModelFeature feature;
  base::StringPiece ukm_metric_name;
  float feature_value;
  int expected_value;
} ClientHostModelFeaturesTestCase;

TEST(OptimizationGuideNavigationDataTest, RecordMetricsNoData) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(/*navigation_id=*/3);
  data.reset();

  // Make sure no UKM recorded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_TRUE(entries.empty());
}

TEST(OptimizationGuideNavigtaionDataTest,
     RecordMetricsRegisteredOptimizationTypes) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(/*navigation_id=*/3);
  data->set_registered_optimization_types(
      {optimization_guide::proto::NOSCRIPT,
       optimization_guide::proto::RESOURCE_LOADING});
  data.reset();

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  EXPECT_TRUE(ukm_recorder.EntryHasMetric(
      entry,
      ukm::builders::OptimizationGuide::kRegisteredOptimizationTypesName));
  // The bitmask should be 110 since NOSCRIPT=1 and RESOURCE_LOADING=2.
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuide::kRegisteredOptimizationTypesName,
      6);
}

TEST(OptimizationGuideNavigtaionDataTest,
     RecordMetricsRegisteredOptimizationTargets) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(/*navigation_id=*/3);
  data->set_registered_optimization_targets(
      {optimization_guide::proto::OPTIMIZATION_TARGET_UNKNOWN,
       optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD});
  data.reset();

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  EXPECT_TRUE(ukm_recorder.EntryHasMetric(
      entry,
      ukm::builders::OptimizationGuide::kRegisteredOptimizationTargetsName));
  // The bitmask should be 11 since UNKNOWN=0 and PAINFUL_PAGE_LOAD=1.
  ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::OptimizationGuide::kRegisteredOptimizationTargetsName, 3);
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsOptimizationTargetModelVersion) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(/*navigation_id=*/3);
  data->SetModelVersionForOptimizationTarget(
      optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, 2);
  data.reset();

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  EXPECT_TRUE(ukm_recorder.EntryHasMetric(
      entry,
      ukm::builders::OptimizationGuide::kPainfulPageLoadModelVersionName));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuide::kPainfulPageLoadModelVersionName,
      2);
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsModelVersionForOptimizationTargetHasNoCorrespondingUkm) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(/*navigation_id=*/3);
  data->SetModelVersionForOptimizationTarget(
      optimization_guide::proto::OPTIMIZATION_TARGET_UNKNOWN, 2);
  data.reset();

  // Make sure UKM not recorded for all empty values.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_TRUE(entries.empty());
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsOptimizationTargetModelPredictionScore) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(/*navigation_id=*/3);
  data->SetModelPredictionScoreForOptimizationTarget(
      optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, 0.123);
  data.reset();

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  EXPECT_TRUE(ukm_recorder.EntryHasMetric(
      entry, ukm::builders::OptimizationGuide::
                 kPainfulPageLoadModelPredictionScoreName));
  ukm_recorder.ExpectEntryMetric(entry,
                                 ukm::builders::OptimizationGuide::
                                     kPainfulPageLoadModelPredictionScoreName,
                                 12);
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsModelPredictionScoreOptimizationTargetHasNoCorrespondingUkm) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(/*navigation_id=*/3);
  data->SetModelPredictionScoreForOptimizationTarget(
      optimization_guide::proto::OPTIMIZATION_TARGET_UNKNOWN, 0.123);
  data.reset();

  // Make sure UKM not recorded for all empty values.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_TRUE(entries.empty());
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsFetchAttemptStatusForNavigation) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(/*navigation_id=*/3);
  data->set_hints_fetch_attempt_status(
      optimization_guide::RaceNavigationFetchAttemptStatus::
          kRaceNavigationFetchHost);
  data.reset();

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  EXPECT_TRUE(ukm_recorder.EntryHasMetric(
      entry, ukm::builders::OptimizationGuide::
                 kNavigationHintsFetchAttemptStatusName));
  ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::OptimizationGuide::kNavigationHintsFetchAttemptStatusName,
      static_cast<int>(optimization_guide::RaceNavigationFetchAttemptStatus::
                           kRaceNavigationFetchHost));
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsFetchInitiatedForNavigation) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(/*navigation_id=*/3);
  base::TimeTicks now = base::TimeTicks::Now();
  data->set_hints_fetch_start(now);
  data->set_hints_fetch_end(now + base::TimeDelta::FromMilliseconds(123));
  data.reset();

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  EXPECT_TRUE(ukm_recorder.EntryHasMetric(
      entry, ukm::builders::OptimizationGuide::
                 kNavigationHintsFetchRequestLatencyName));
  ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::OptimizationGuide::kNavigationHintsFetchRequestLatencyName,
      123);
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsFetchInitiatedForNavigationNoStart) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(/*navigation_id=*/3);
  base::TimeTicks now = base::TimeTicks::Now();
  data->set_hints_fetch_end(now);
  data.reset();

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_TRUE(entries.empty());
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsFetchInitiatedForNavigationNoEnd) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(/*navigation_id=*/3);
  base::TimeTicks now = base::TimeTicks::Now();
  data->set_hints_fetch_start(now);
  data.reset();

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  EXPECT_TRUE(ukm_recorder.EntryHasMetric(
      entry, ukm::builders::OptimizationGuide::
                 kNavigationHintsFetchRequestLatencyName));
  ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::OptimizationGuide::kNavigationHintsFetchRequestLatencyName,
      INT64_MAX);
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsFetchInitiatedForNavigationEndLessThanStart) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(/*navigation_id=*/3);
  base::TimeTicks now = base::TimeTicks::Now();
  data->set_hints_fetch_start(now);
  data->set_hints_fetch_end(now - base::TimeDelta::FromMilliseconds(123));
  data.reset();

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  EXPECT_TRUE(ukm_recorder.EntryHasMetric(
      entry, ukm::builders::OptimizationGuide::
                 kNavigationHintsFetchRequestLatencyName));
  ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::OptimizationGuide::kNavigationHintsFetchRequestLatencyName,
      INT64_MAX);
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsPredictionModelHostModelFeatures) {
  base::test::TaskEnvironment env;
  ClientHostModelFeaturesTestCase test_cases[] = {
      {optimization_guide::proto::
           CLIENT_MODEL_FEATURE_FIRST_CONTENTFUL_PAINT_SESSION_MEAN,
       ukm::builders::OptimizationGuide::
           kPredictionModelFeatureNavigationToFCPSessionMeanName,
       2.0, 2},
      {optimization_guide::proto::
           CLIENT_MODEL_FEATURE_FIRST_CONTENTFUL_PAINT_SESSION_STANDARD_DEVIATION,
       ukm::builders::OptimizationGuide::
           kPredictionModelFeatureNavigationToFCPSessionStdDevName,
       3.0, 3},
      {optimization_guide::proto::CLIENT_MODEL_FEATURE_PAGE_TRANSITION,
       ukm::builders::OptimizationGuide::
           kPredictionModelFeaturePageTransitionName,
       20.0, 20},
      {optimization_guide::proto::CLIENT_MODEL_FEATURE_SAME_ORIGIN_NAVIGATION,
       ukm::builders::OptimizationGuide::
           kPredictionModelFeatureIsSameOriginNavigationName,
       1.0, 1},
      {optimization_guide::proto::CLIENT_MODEL_FEATURE_SITE_ENGAGEMENT_SCORE,
       ukm::builders::OptimizationGuide::
           kPredictionModelFeatureSiteEngagementScoreName,
       5.5, 10},
      {optimization_guide::proto::
           CLIENT_MODEL_FEATURE_EFFECTIVE_CONNECTION_TYPE,
       ukm::builders::OptimizationGuide::
           kPredictionModelFeatureEffectiveConnectionTypeName,
       3.0, 3},
      {optimization_guide::proto::
           CLIENT_MODEL_FEATURE_FIRST_CONTENTFUL_PAINT_PREVIOUS_PAGE_LOAD,
       ukm::builders::OptimizationGuide::
           kPredictionModelFeaturePreviousPageLoadNavigationToFCPName,
       200.0, 200},
  };

  for (const auto& test_case : test_cases) {
    ukm::TestAutoSetUkmRecorder ukm_recorder;
    std::unique_ptr<OptimizationGuideNavigationData> data =
        std::make_unique<OptimizationGuideNavigationData>(/*navigation_id=*/3);
    data->SetValueForModelFeature(test_case.feature, test_case.feature_value);
    data.reset();

    auto entries = ukm_recorder.GetEntriesByName(
        ukm::builders::OptimizationGuide::kEntryName);
    EXPECT_EQ(1u, entries.size());
    auto* entry = entries[0];
    EXPECT_TRUE(ukm_recorder.EntryHasMetric(entry, test_case.ukm_metric_name));
    ukm_recorder.ExpectEntryMetric(entry, test_case.ukm_metric_name,
                                   test_case.expected_value);
  }
}
