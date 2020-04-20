// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_navigation_data.h"

#include <memory>

#include "base/base64.h"
#include "base/test/metrics/histogram_tester.h"
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

TEST(OptimizationGuideNavigationDataTest, RecordMetricsNoDataNoCommit) {
  base::test::TaskEnvironment env;

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(/*navigation_id=*/3);
  data.reset();

  // Make sure no UMA recorded.
  EXPECT_THAT(
      histogram_tester.GetAllHistogramsRecorded(),
      Not(AnyOf(
          HasSubstr("OptimizationGuide.ApplyDecision"),
          HasSubstr("OptimizationGuide.TargetDecision"))));

  // Make sure no UKM recorded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_TRUE(entries.empty());
}

TEST(OptimizationGuideNavigationDataTest, RecordMetricsBadHintVersion) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(/*navigation_id=*/3);
  data->set_serialized_hint_version_string("123");
  data.reset();

  // Make sure no UKM recorded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_TRUE(entries.empty());
}

TEST(OptimizationGuideNavigationDataTest, RecordMetricsEmptyHintVersion) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(/*navigation_id=*/3);
  data->set_serialized_hint_version_string("");
  data.reset();

  // Make sure no UKM recorded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_TRUE(entries.empty());
}

TEST(OptimizationGuideNavigationDataTest, RecordMetricsZeroTimestampOrSource) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(/*navigation_id=*/3);
  optimization_guide::proto::Version hint_version;
  hint_version.mutable_generation_timestamp()->set_seconds(0);
  hint_version.set_hint_source(optimization_guide::proto::HINT_SOURCE_UNKNOWN);
  std::string hint_version_string;
  hint_version.SerializeToString(&hint_version_string);
  base::Base64Encode(hint_version_string, &hint_version_string);
  data->set_serialized_hint_version_string(hint_version_string);
  data.reset();

  // Make sure UKM not recorded for all empty values.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_TRUE(entries.empty());
}

TEST(OptimizationGuideNavigationDataTest, RecordMetricsGoodHintVersion) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(/*navigation_id=*/3);
  optimization_guide::proto::Version hint_version;
  hint_version.mutable_generation_timestamp()->set_seconds(234);
  hint_version.set_hint_source(
      optimization_guide::proto::HINT_SOURCE_OPTIMIZATION_GUIDE_SERVICE);
  std::string hint_version_string;
  hint_version.SerializeToString(&hint_version_string);
  base::Base64Encode(hint_version_string, &hint_version_string);
  data->set_serialized_hint_version_string(hint_version_string);
  data.reset();

  // Make sure version is serialized properly and UKM is recorded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuide::kHintSourceName,
      static_cast<int>(
          optimization_guide::proto::HINT_SOURCE_OPTIMIZATION_GUIDE_SERVICE));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuide::kHintGenerationTimestampName,
      234);
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsHintVersionWithUnknownSource) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(/*navigation_id=*/3);
  optimization_guide::proto::Version hint_version;
  hint_version.mutable_generation_timestamp()->set_seconds(234);
  hint_version.set_hint_source(optimization_guide::proto::HINT_SOURCE_UNKNOWN);
  std::string hint_version_string;
  hint_version.SerializeToString(&hint_version_string);
  base::Base64Encode(hint_version_string, &hint_version_string);
  data->set_serialized_hint_version_string(hint_version_string);
  data.reset();

  // Make sure version is serialized properly and UKM is only recorded for
  // non-empty values.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  EXPECT_FALSE(ukm_recorder.EntryHasMetric(
      entry, ukm::builders::OptimizationGuide::kHintSourceName));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuide::kHintGenerationTimestampName,
      234);
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsHintVersionWithNoSource) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(/*navigation_id=*/3);
  optimization_guide::proto::Version hint_version;
  hint_version.mutable_generation_timestamp()->set_seconds(234);
  std::string hint_version_string;
  hint_version.SerializeToString(&hint_version_string);
  base::Base64Encode(hint_version_string, &hint_version_string);
  data->set_serialized_hint_version_string(hint_version_string);
  data.reset();

  // Make sure version is serialized properly and UKM is recorded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  EXPECT_FALSE(ukm_recorder.EntryHasMetric(
      entry, ukm::builders::OptimizationGuide::kHintSourceName));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuide::kHintGenerationTimestampName,
      234);
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsHintVersionWithZeroTimestamp) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(/*navigation_id=*/3);
  optimization_guide::proto::Version hint_version;
  hint_version.mutable_generation_timestamp()->set_seconds(0);
  hint_version.set_hint_source(
      optimization_guide::proto::HINT_SOURCE_OPTIMIZATION_GUIDE_SERVICE);
  std::string hint_version_string;
  hint_version.SerializeToString(&hint_version_string);
  base::Base64Encode(hint_version_string, &hint_version_string);
  data->set_serialized_hint_version_string(hint_version_string);
  data.reset();

  // Make sure version is serialized properly and UKM is only recorded for
  // non-empty values.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  EXPECT_FALSE(ukm_recorder.EntryHasMetric(
      entry, ukm::builders::OptimizationGuide::kHintGenerationTimestampName));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuide::kHintSourceName,
      static_cast<int>(
          optimization_guide::proto::HINT_SOURCE_OPTIMIZATION_GUIDE_SERVICE));
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsHintVersionWithNoTimestamp) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(/*navigation_id=*/3);
  optimization_guide::proto::Version hint_version;
  hint_version.set_hint_source(
      optimization_guide::proto::HINT_SOURCE_OPTIMIZATION_GUIDE_SERVICE);
  std::string hint_version_string;
  hint_version.SerializeToString(&hint_version_string);
  base::Base64Encode(hint_version_string, &hint_version_string);
  data->set_serialized_hint_version_string(hint_version_string);
  data.reset();

  // Make sure version is serialized properly and UKM is recorded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  EXPECT_FALSE(ukm_recorder.EntryHasMetric(
      entry, ukm::builders::OptimizationGuide::kHintGenerationTimestampName));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuide::kHintSourceName,
      static_cast<int>(
          optimization_guide::proto::HINT_SOURCE_OPTIMIZATION_GUIDE_SERVICE));
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
     RecordMetricsHintCoverageHasHintBeforeCommitNoFetch) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(/*navigation_id=*/3);
  data->set_has_hint_before_commit(true);
  data.reset();

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  EXPECT_TRUE(ukm_recorder.EntryHasMetric(
      entry, ukm::builders::OptimizationGuide::kNavigationHostCoveredName));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuide::kNavigationHostCoveredName,
      static_cast<int>(
          optimization_guide::NavigationHostCoveredStatus::kCovered));
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsHintCoverageHasHintAfterCommitNoFetch) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(/*navigation_id=*/3);
  data->set_has_hint_after_commit(true);
  data.reset();

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  EXPECT_TRUE(ukm_recorder.EntryHasMetric(
      entry, ukm::builders::OptimizationGuide::kNavigationHostCoveredName));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuide::kNavigationHostCoveredName,
      static_cast<int>(
          optimization_guide::NavigationHostCoveredStatus::kCovered));
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsHintCoverageNoHintHasFetchBeforeCommit) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(/*navigation_id=*/3);
  data->set_has_hint_before_commit(false);
  data->set_was_host_covered_by_fetch_at_navigation_start(true);
  data.reset();

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  EXPECT_TRUE(ukm_recorder.EntryHasMetric(
      entry, ukm::builders::OptimizationGuide::kNavigationHostCoveredName));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuide::kNavigationHostCoveredName,
      static_cast<int>(
          optimization_guide::NavigationHostCoveredStatus::kCovered));
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsHintCoverageNoHintHasFetchAtCommit) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(/*navigation_id=*/3);
  data->set_has_hint_after_commit(false);
  data->set_was_host_covered_by_fetch_at_commit(true);
  data.reset();

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  EXPECT_TRUE(ukm_recorder.EntryHasMetric(
      entry, ukm::builders::OptimizationGuide::kNavigationHostCoveredName));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuide::kNavigationHostCoveredName,
      static_cast<int>(
          optimization_guide::NavigationHostCoveredStatus::kCovered));
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsHintCoverageNoHintOrFetchBeforeCommitAndNoFetchAttempted) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(/*navigation_id=*/3);
  data->set_has_hint_before_commit(false);
  data->set_was_host_covered_by_fetch_at_navigation_start(false);
  data.reset();

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  EXPECT_TRUE(ukm_recorder.EntryHasMetric(
      entry, ukm::builders::OptimizationGuide::kNavigationHostCoveredName));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuide::kNavigationHostCoveredName,
      static_cast<int>(
          optimization_guide::NavigationHostCoveredStatus::kFetchNotAttempted));
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsHintCoverageNoHintOrFetchAtCommitAndNoFetchAttempted) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(/*navigation_id=*/3);
  data->set_has_hint_after_commit(false);
  data->set_was_host_covered_by_fetch_at_commit(false);
  data.reset();

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  EXPECT_TRUE(ukm_recorder.EntryHasMetric(
      entry, ukm::builders::OptimizationGuide::kNavigationHostCoveredName));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuide::kNavigationHostCoveredName,
      static_cast<int>(
          optimization_guide::NavigationHostCoveredStatus::kFetchNotAttempted));
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsHintCoverageNoHintOrFetchBeforeCommitButFetchAttempted) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(/*navigation_id=*/3);
  data->set_has_hint_before_commit(false);
  data->set_was_host_covered_by_fetch_at_navigation_start(false);
  data->set_was_hint_for_host_attempted_to_be_fetched(true);
  data.reset();

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  EXPECT_TRUE(ukm_recorder.EntryHasMetric(
      entry, ukm::builders::OptimizationGuide::kNavigationHostCoveredName));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuide::kNavigationHostCoveredName,
      static_cast<int>(optimization_guide::NavigationHostCoveredStatus::
                           kFetchNotSuccessful));
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsHintCoverageNoHintOrFetchAtCommitButFetchAttempted) {
  base::test::TaskEnvironment env;

  ukm::TestAutoSetUkmRecorder ukm_recorder;

  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(/*navigation_id=*/3);
  data->set_has_hint_after_commit(false);
  data->set_was_host_covered_by_fetch_at_commit(false);
  data->set_was_hint_for_host_attempted_to_be_fetched(true);
  data.reset();

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0];
  EXPECT_TRUE(ukm_recorder.EntryHasMetric(
      entry, ukm::builders::OptimizationGuide::kNavigationHostCoveredName));
  ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::OptimizationGuide::kNavigationHostCoveredName,
      static_cast<int>(optimization_guide::NavigationHostCoveredStatus::
                           kFetchNotSuccessful));
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
     RecordMetricsMultipleOptimizationTypes) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(/*navigation_id=*/3);
  data->SetDecisionForOptimizationType(
      optimization_guide::proto::NOSCRIPT,
      optimization_guide::OptimizationTypeDecision::kAllowedByHint);
  data->SetDecisionForOptimizationType(
      optimization_guide::proto::DEFER_ALL_SCRIPT,
      optimization_guide::OptimizationTypeDecision::
          kAllowedByOptimizationFilter);
  data.reset();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecision.NoScript",
      static_cast<int>(
          optimization_guide::OptimizationTypeDecision::kAllowedByHint),
      1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecision.DeferAllScript",
      static_cast<int>(optimization_guide::OptimizationTypeDecision::
                           kAllowedByOptimizationFilter),
      1);
}

TEST(OptimizationGuideNavigationDataTest, RecordMetricsRecordsLatestType) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(/*navigation_id=*/3);
  data->SetDecisionForOptimizationType(
      optimization_guide::proto::NOSCRIPT,
      optimization_guide::OptimizationTypeDecision::kAllowedByHint);
  data->SetDecisionForOptimizationType(
      optimization_guide::proto::NOSCRIPT,
      optimization_guide::OptimizationTypeDecision::
          kAllowedByOptimizationFilter);
  data.reset();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecision.NoScript",
      static_cast<int>(optimization_guide::OptimizationTypeDecision::
                           kAllowedByOptimizationFilter),
      1);
}

TEST(OptimizationGuideNavigationDataTest,
     RecordMetricsMultipleOptimizationTargets) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(/*navigation_id=*/3);
  data->SetDecisionForOptimizationTarget(
      optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      optimization_guide::OptimizationTargetDecision::kPageLoadMatches);
  data->SetDecisionForOptimizationTarget(
      optimization_guide::proto::OPTIMIZATION_TARGET_UNKNOWN,
      optimization_guide::OptimizationTargetDecision::kPageLoadDoesNotMatch);
  data.reset();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.TargetDecision.PainfulPageLoad",
      static_cast<int>(
          optimization_guide::OptimizationTargetDecision::kPageLoadMatches),
      1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.TargetDecision.Unknown",
      static_cast<int>(optimization_guide::OptimizationTargetDecision::
                           kPageLoadDoesNotMatch),
      1);
}

TEST(OptimizationGuideNavigationDataTest, RecordMetricsRecordsLatestTarget) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<OptimizationGuideNavigationData> data =
      std::make_unique<OptimizationGuideNavigationData>(/*navigation_id=*/3);
  data->SetDecisionForOptimizationTarget(
      optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      optimization_guide::OptimizationTargetDecision::kPageLoadDoesNotMatch);
  data->SetDecisionForOptimizationTarget(
      optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      optimization_guide::OptimizationTargetDecision::kPageLoadMatches);
  data.reset();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.TargetDecision.PainfulPageLoad",
      static_cast<int>(
          optimization_guide::OptimizationTargetDecision::kPageLoadMatches),
      1);
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
