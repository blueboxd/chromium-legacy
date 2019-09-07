// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/histogram_util.h"

#include "base/containers/flat_set.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/ranking_item_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {

class HistogramUtilTest : public testing::Test {
 public:
  HistogramUtilTest() {}
  ~HistogramUtilTest() override {}

  const base::HistogramTester histogram_tester_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HistogramUtilTest);
};

TEST_F(HistogramUtilTest, TestLaunchedItemPosition) {
  std::vector<RankingItemType> result_types = {
      RankingItemType::kOmniboxGeneric, RankingItemType::kZeroStateFile,
      RankingItemType::kZeroStateFile, RankingItemType::kZeroStateFile};

  // Don't log if there is no click.
  LogZeroStateResultsListMetrics(result_types, -1);
  histogram_tester_.ExpectTotalCount(
      "Apps.AppList.ZeroStateResultsList.LaunchedItemPosition", 0);

  // Log some actual clicks.
  LogZeroStateResultsListMetrics(result_types, 3);
  histogram_tester_.ExpectBucketCount(
      "Apps.AppList.ZeroStateResultsList.LaunchedItemPosition", 3, 1);

  LogZeroStateResultsListMetrics(result_types, 2);
  histogram_tester_.ExpectBucketCount(
      "Apps.AppList.ZeroStateResultsList.LaunchedItemPosition", 2, 1);
}

TEST_F(HistogramUtilTest, TestNumImpressionTypes) {
  // No results.
  std::vector<RankingItemType> result_types_1;
  LogZeroStateResultsListMetrics(result_types_1, 0);
  histogram_tester_.ExpectBucketCount(
      "Apps.AppList.ZeroStateResultsList.NumImpressionTypes", 0, 1);

  // Several types of results.
  std::vector<RankingItemType> result_types_2 = {
      RankingItemType::kOmniboxGeneric, RankingItemType::kZeroStateFile,
      RankingItemType::kDriveQuickAccess};
  LogZeroStateResultsListMetrics(result_types_2, 0);
  histogram_tester_.ExpectBucketCount(
      "Apps.AppList.ZeroStateResultsList.NumImpressionTypes", 3, 1);

  // Some types doubled up.
  std::vector<RankingItemType> result_types_3 = {
      RankingItemType::kOmniboxGeneric, RankingItemType::kZeroStateFile,
      RankingItemType::kZeroStateFile, RankingItemType::kZeroStateFile};
  LogZeroStateResultsListMetrics(result_types_3, 0);
  histogram_tester_.ExpectBucketCount(
      "Apps.AppList.ZeroStateResultsList.NumImpressionTypes", 2, 1);
}

}  // namespace app_list
