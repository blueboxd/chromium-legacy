// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "chrome/services/local_search_service/index_impl.h"
#include "chrome/services/local_search_service/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace local_search_service {

namespace {
// Search parameters with default values.
constexpr double kDefaultRelevanceThreshold = 0.3;
constexpr double kDefaultPartialMatchPenaltyRate = 0.9;
constexpr bool kDefaultUsePrefixOnly = false;
constexpr bool kDefaultUseWeightedRatio = true;
constexpr bool kDefaultUseEditDistance = false;

}  // namespace

class IndexImplTest : public testing::Test {
 protected:
  IndexImpl index_impl_;
};

TEST_F(IndexImplTest, SetSearchParams) {
  {
    // No params are specified so default values are used.
    const SearchParams search_params;
    index_impl_.SetSearchParams(search_params);

    // Initialize them to values different from default.
    double relevance_threshold = kDefaultRelevanceThreshold * 2.0;
    double partial_match_penalty_rate = kDefaultPartialMatchPenaltyRate * 2.0;
    bool use_prefix_only = !kDefaultUsePrefixOnly;
    bool use_weighted_ratio = !kDefaultUseWeightedRatio;
    bool use_edit_distance = !kDefaultUseEditDistance;

    index_impl_.GetSearchParamsForTesting(
        &relevance_threshold, &partial_match_penalty_rate, &use_prefix_only,
        &use_weighted_ratio, &use_edit_distance);

    EXPECT_DOUBLE_EQ(relevance_threshold, kDefaultRelevanceThreshold);
    EXPECT_DOUBLE_EQ(partial_match_penalty_rate,
                     kDefaultPartialMatchPenaltyRate);
    EXPECT_EQ(use_prefix_only, kDefaultUsePrefixOnly);
    EXPECT_EQ(use_weighted_ratio, kDefaultUseWeightedRatio);
    EXPECT_EQ(use_edit_distance, kDefaultUseEditDistance);
  }

  {
    // Params are specified and are used.
    const SearchParams search_params = {
        kDefaultRelevanceThreshold / 2, kDefaultPartialMatchPenaltyRate / 2,
        !kDefaultUsePrefixOnly, !kDefaultUseWeightedRatio,
        !kDefaultUseEditDistance};

    index_impl_.SetSearchParams(search_params);

    // Initialize them to default values.
    double relevance_threshold = kDefaultRelevanceThreshold;
    double partial_match_penalty_rate = kDefaultPartialMatchPenaltyRate;
    bool use_prefix_only = kDefaultUsePrefixOnly;
    bool use_weighted_ratio = kDefaultUseWeightedRatio;
    bool use_edit_distance = kDefaultUseEditDistance;

    index_impl_.GetSearchParamsForTesting(
        &relevance_threshold, &partial_match_penalty_rate, &use_prefix_only,
        &use_weighted_ratio, &use_edit_distance);

    EXPECT_DOUBLE_EQ(relevance_threshold, kDefaultRelevanceThreshold / 2);
    EXPECT_DOUBLE_EQ(partial_match_penalty_rate,
                     kDefaultPartialMatchPenaltyRate / 2);
    EXPECT_EQ(use_prefix_only, !kDefaultUsePrefixOnly);
    EXPECT_EQ(use_weighted_ratio, !kDefaultUseWeightedRatio);
    EXPECT_EQ(use_edit_distance, !kDefaultUseEditDistance);
  }
}

TEST_F(IndexImplTest, RelevanceThreshold) {
  const std::map<std::string, std::vector<std::string>> data_to_register = {
      {"id1", {"Clash Of Clan"}}, {"id2", {"famous"}}};
  std::vector<Data> data = CreateTestData(data_to_register);
  index_impl_.AddOrUpdate(data);
  EXPECT_EQ(index_impl_.GetSize(), 2u);
  {
    SearchParams search_params;
    search_params.relevance_threshold = 0.0;
    index_impl_.SetSearchParams(search_params);

    FindAndCheck(&index_impl_, "CC",
                 /*max_results=*/-1, ResponseStatus::kSuccess, {"id1", "id2"});
  }
  {
    SearchParams search_params;
    search_params.relevance_threshold = 0.3;
    index_impl_.SetSearchParams(search_params);

    FindAndCheck(&index_impl_, "CC",
                 /*max_results=*/-1, ResponseStatus::kSuccess, {"id1"});
  }
  {
    SearchParams search_params;
    search_params.relevance_threshold = 0.9;
    index_impl_.SetSearchParams(search_params);

    FindAndCheck(&index_impl_, "CC",
                 /*max_results=*/-1, ResponseStatus::kSuccess, {});
  }
}

TEST_F(IndexImplTest, MaxResults) {
  const std::map<std::string, std::vector<std::string>> data_to_register = {
      {"id1", {"Clash Of Clan"}}, {"id2", {"famous"}}};
  std::vector<Data> data = CreateTestData(data_to_register);
  index_impl_.AddOrUpdate(data);
  EXPECT_EQ(index_impl_.GetSize(), 2u);
  SearchParams search_params;
  search_params.relevance_threshold = 0.0;
  index_impl_.SetSearchParams(search_params);

  FindAndCheck(&index_impl_, "CC",
               /*max_results=*/-1, ResponseStatus::kSuccess, {"id1", "id2"});
  FindAndCheck(&index_impl_, "CC",
               /*max_results=*/1, ResponseStatus::kSuccess, {"id1"});
}

}  // namespace local_search_service
