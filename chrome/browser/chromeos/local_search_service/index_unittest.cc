// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/local_search_service/index.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace local_search_service {

namespace {

// This is (data-id, content-ids).
using ResultWithIds = std::pair<std::string, std::vector<std::string>>;
using ContentWithId = std::pair<std::string, std::string>;

// Creates test data to be registered to the index. |input| is a map from
// id to contents (id and content).
std::vector<Data> CreateTestData(
    const std::map<std::string, std::vector<ContentWithId>>& input) {
  std::vector<Data> output;
  for (const auto& item : input) {
    Data data;
    data.id = item.first;
    std::vector<Content>& contents = data.contents;
    for (const auto& content_with_id : item.second) {
      const Content content(content_with_id.first,
                            base::UTF8ToUTF16(content_with_id.second));
      contents.push_back(content);
    }
    output.push_back(data);
  }
  return output;
}

void CheckSearchParams(const SearchParams& actual,
                       const SearchParams& expected) {
  EXPECT_DOUBLE_EQ(actual.relevance_threshold, expected.relevance_threshold);
  EXPECT_DOUBLE_EQ(actual.partial_match_penalty_rate,
                   expected.partial_match_penalty_rate);
  EXPECT_EQ(actual.use_prefix_only, expected.use_prefix_only);
  EXPECT_EQ(actual.use_edit_distance, expected.use_edit_distance);
}

void FindAndCheckResults(Index* index,
                         std::string query,
                         int32_t max_results,
                         ResponseStatus expected_status,
                         const std::vector<ResultWithIds>& expected_results) {
  DCHECK(index);

  std::vector<Result> results;
  auto status = index->Find(base::UTF8ToUTF16(query), max_results, &results);

  EXPECT_EQ(status, expected_status);

  if (!results.empty()) {
    // If results are returned, check size and values match the expected.
    EXPECT_EQ(results.size(), expected_results.size());
    for (size_t i = 0; i < results.size(); ++i) {
      EXPECT_EQ(results[i].id, expected_results[i].first);
      EXPECT_EQ(results[i].positions.size(), expected_results[i].second.size());

      for (size_t j = 0; j < results[i].positions.size(); ++j) {
        EXPECT_EQ(results[i].positions[j].content_id,
                  expected_results[i].second[j]);
      }
      // Scores should be non-increasing.
      if (i < results.size() - 1) {
        EXPECT_GE(results[i].score, results[i + 1].score);
      }
    }
    return;
  }

  // If no results are returned, expected ids should be empty.
  EXPECT_TRUE(expected_results.empty());
}

}  // namespace

class IndexTest : public testing::Test {
 protected:
  Index index_;
};

TEST_F(IndexTest, SetSearchParams) {
  {
    // No params are specified so default values are used.
    const SearchParams used_params = index_.GetSearchParamsForTesting();

    CheckSearchParams(used_params, SearchParams());
  }

  {
    // Params are specified and are used.
    SearchParams search_params;
    const SearchParams default_params;
    search_params.relevance_threshold = default_params.relevance_threshold / 2;
    search_params.partial_match_penalty_rate =
        default_params.partial_match_penalty_rate / 2;
    search_params.use_prefix_only = !default_params.use_prefix_only;
    search_params.use_edit_distance = !default_params.use_edit_distance;

    index_.SetSearchParams(search_params);

    const SearchParams used_params = index_.GetSearchParamsForTesting();

    CheckSearchParams(used_params, search_params);
  }
}

TEST_F(IndexTest, RelevanceThreshold) {
  const std::map<std::string, std::vector<ContentWithId>> data_to_register = {
      {"id1", {{"tag1", "Wi-Fi"}}}, {"id2", {{"tag2", "famous"}}}};
  std::vector<Data> data = CreateTestData(data_to_register);
  index_.AddOrUpdate(data);
  EXPECT_EQ(index_.GetSize(), 2u);
  {
    SearchParams search_params;
    search_params.relevance_threshold = 0.0;
    index_.SetSearchParams(search_params);

    const std::vector<ResultWithIds> expected_results = {{"id1", {"tag1"}},
                                                         {"id2", {"tag2"}}};
    FindAndCheckResults(&index_, "wifi",
                        /*max_results=*/-1, ResponseStatus::kSuccess,
                        expected_results);
  }
  {
    SearchParams search_params;
    search_params.relevance_threshold = 0.3;
    index_.SetSearchParams(search_params);

    const std::vector<ResultWithIds> expected_results = {{"id1", {"tag1"}}};
    FindAndCheckResults(&index_, "wifi",
                        /*max_results=*/-1, ResponseStatus::kSuccess,
                        expected_results);
  }
  {
    SearchParams search_params;
    search_params.relevance_threshold = 0.9;
    index_.SetSearchParams(search_params);

    FindAndCheckResults(&index_, "wifi",
                        /*max_results=*/-1, ResponseStatus::kSuccess, {});
  }
}

TEST_F(IndexTest, MaxResults) {
  const std::map<std::string, std::vector<ContentWithId>> data_to_register = {
      {"id1", {{"tag1", "abcde"}, {"tag2", "Wi-Fi"}}},
      {"id2", {{"tag3", "wifi"}}}};
  std::vector<Data> data = CreateTestData(data_to_register);
  index_.AddOrUpdate(data);
  EXPECT_EQ(index_.GetSize(), 2u);
  SearchParams search_params;
  search_params.relevance_threshold = 0.3;
  index_.SetSearchParams(search_params);

  {
    const std::vector<ResultWithIds> expected_results = {{"id2", {"tag3"}},
                                                         {"id1", {"tag2"}}};
    FindAndCheckResults(&index_, "wifi",
                        /*max_results=*/-1, ResponseStatus::kSuccess,
                        expected_results);
  }
  {
    const std::vector<ResultWithIds> expected_results = {{"id2", {"tag3"}}};
    FindAndCheckResults(&index_, "wifi",
                        /*max_results=*/1, ResponseStatus::kSuccess,
                        expected_results);
  }
}

TEST_F(IndexTest, ResultFound) {
  const std::map<std::string, std::vector<ContentWithId>> data_to_register = {
      {"id1", {{"cid1", "id1"}, {"cid2", "tag1a"}, {"cid3", "tag1b"}}},
      {"xyz", {{"cid4", "xyz"}}}};
  std::vector<Data> data = CreateTestData(data_to_register);
  EXPECT_EQ(data.size(), 2u);

  index_.AddOrUpdate(data);
  EXPECT_EQ(index_.GetSize(), 2u);

  // Find result with query "id1". It returns an exact match.
  const std::vector<ResultWithIds> expected_results = {{"id1", {"cid1"}}};
  FindAndCheckResults(&index_, "id1",
                      /*max_results=*/-1, ResponseStatus::kSuccess,
                      expected_results);
  FindAndCheckResults(&index_, "abc",
                      /*max_results=*/-1, ResponseStatus::kSuccess, {});
}

}  // namespace local_search_service
