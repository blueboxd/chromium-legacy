// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/services/local_search_service/index_impl.h"
#include "chrome/services/local_search_service/public/mojom/local_search_service.mojom-test-utils.h"
#include "chrome/services/local_search_service/public/mojom/types.mojom.h"
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

void SetSearchParamsAndCheck(mojom::Index* index,
                             mojom::SearchParamsPtr search_params) {
  DCHECK(index);
  mojom::IndexAsyncWaiter(index).SetSearchParams(std::move(search_params));
}

}  // namespace

class IndexImplTest : public testing::Test {
 public:
  IndexImplTest() {
    index_impl_.BindReceiver(index_remote_.BindNewPipeAndPassReceiver());
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  IndexImpl index_impl_;
  mojo::Remote<mojom::Index> index_remote_;
};

TEST_F(IndexImplTest, SetSearchParams) {
  {
    // No params are specified so default values are used.
    mojom::SearchParamsPtr search_params = mojom::SearchParams::New();
    SetSearchParamsAndCheck(index_remote_.get(), std::move(search_params));

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
    mojom::SearchParamsPtr search_params = mojom::SearchParams::New(
        kDefaultRelevanceThreshold / 2, kDefaultPartialMatchPenaltyRate / 2,
        !kDefaultUsePrefixOnly, !kDefaultUseWeightedRatio,
        !kDefaultUseEditDistance);

    SetSearchParamsAndCheck(index_remote_.get(), std::move(search_params));

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
  std::vector<mojom::DataPtr> data = CreateTestData(data_to_register);
  AddOrUpdateAndCheck(index_remote_.get(), std::move(data));
  GetSizeAndCheck(index_remote_.get(), 2u);
  {
    mojom::SearchParamsPtr search_params = mojom::SearchParams::New();
    search_params->relevance_threshold = 0.0;
    SetSearchParamsAndCheck(index_remote_.get(), std::move(search_params));

    FindAndCheck(index_remote_.get(), "CC", /*max_latency_in_ms=*/-1,
                 /*max_results=*/-1, mojom::ResponseStatus::SUCCESS,
                 {"id1", "id2"});
  }
  {
    mojom::SearchParamsPtr search_params = mojom::SearchParams::New();
    search_params->relevance_threshold = 0.3;
    SetSearchParamsAndCheck(index_remote_.get(), std::move(search_params));

    FindAndCheck(index_remote_.get(), "CC", /*max_latency_in_ms=*/-1,
                 /*max_results=*/-1, mojom::ResponseStatus::SUCCESS, {"id1"});
  }
  {
    mojom::SearchParamsPtr search_params = mojom::SearchParams::New();
    search_params->relevance_threshold = 0.9;
    SetSearchParamsAndCheck(index_remote_.get(), std::move(search_params));

    FindAndCheck(index_remote_.get(), "CC", /*max_latency_in_ms=*/-1,
                 /*max_results=*/-1, mojom::ResponseStatus::SUCCESS, {});
  }
}

TEST_F(IndexImplTest, MaxResults) {
  const std::map<std::string, std::vector<std::string>> data_to_register = {
      {"id1", {"Clash Of Clan"}}, {"id2", {"famous"}}};
  std::vector<mojom::DataPtr> data = CreateTestData(data_to_register);
  AddOrUpdateAndCheck(index_remote_.get(), std::move(data));
  GetSizeAndCheck(index_remote_.get(), 2u);
  mojom::SearchParamsPtr search_params = mojom::SearchParams::New();
  search_params->relevance_threshold = 0.0;
  SetSearchParamsAndCheck(index_remote_.get(), std::move(search_params));

  FindAndCheck(index_remote_.get(), "CC", /*max_latency_in_ms=*/-1,
               /*max_results=*/-1, mojom::ResponseStatus::SUCCESS,
               {"id1", "id2"});
  FindAndCheck(index_remote_.get(), "CC", /*max_latency_in_ms=*/-1,
               /*max_results=*/1, mojom::ResponseStatus::SUCCESS, {"id1"});
}

}  // namespace local_search_service
