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
#include "chrome/browser/chromeos/local_search_service/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace local_search_service {

namespace {

void CheckSearchParams(const SearchParams& actual,
                       const SearchParams& expected) {
  EXPECT_DOUBLE_EQ(actual.relevance_threshold, expected.relevance_threshold);
  EXPECT_DOUBLE_EQ(actual.partial_match_penalty_rate,
                   expected.partial_match_penalty_rate);
  EXPECT_EQ(actual.use_prefix_only, expected.use_prefix_only);
  EXPECT_EQ(actual.use_edit_distance, expected.use_edit_distance);
  EXPECT_EQ(actual.split_search_tags, expected.split_search_tags);
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
    search_params.split_search_tags = !default_params.split_search_tags;

    index_.SetSearchParams(search_params);

    const SearchParams used_params = index_.GetSearchParamsForTesting();

    CheckSearchParams(used_params, search_params);
  }
}

TEST_F(IndexTest, RelevanceThreshold) {
  const std::map<std::string, std::vector<std::string>> data_to_register = {
      {"id1", {"Wi-Fi"}}, {"id2", {"famous"}}};
  std::vector<Data> data = CreateTestData(data_to_register);
  index_.AddOrUpdate(data);
  EXPECT_EQ(index_.GetSize(), 2u);
  {
    SearchParams search_params;
    search_params.relevance_threshold = 0.0;
    index_.SetSearchParams(search_params);

    FindAndCheck(&index_, "wifi",
                 /*max_results=*/-1, ResponseStatus::kSuccess, {"id1", "id2"});
  }
  {
    SearchParams search_params;
    search_params.relevance_threshold = 0.3;
    index_.SetSearchParams(search_params);

    FindAndCheck(&index_, "wifi",
                 /*max_results=*/-1, ResponseStatus::kSuccess, {"id1"});
  }
  {
    SearchParams search_params;
    search_params.relevance_threshold = 0.9;
    index_.SetSearchParams(search_params);

    FindAndCheck(&index_, "wifi",
                 /*max_results=*/-1, ResponseStatus::kSuccess, {});
  }
}

TEST_F(IndexTest, MaxResults) {
  const std::map<std::string, std::vector<std::string>> data_to_register = {
      {"id1", {"Wi-Fi"}}, {"id2", {"famous"}}};
  std::vector<Data> data = CreateTestData(data_to_register);
  index_.AddOrUpdate(data);
  EXPECT_EQ(index_.GetSize(), 2u);
  SearchParams search_params;
  // Set relevance threshold to 0 to ensure everything matches.
  search_params.relevance_threshold = 0.0;
  index_.SetSearchParams(search_params);

  FindAndCheck(&index_, "wifi",
               /*max_results=*/-1, ResponseStatus::kSuccess, {"id1", "id2"});
  FindAndCheck(&index_, "wifi",
               /*max_results=*/1, ResponseStatus::kSuccess, {"id1"});
}

TEST_F(IndexTest, ResultFound) {
  const std::map<std::string, std::vector<std::string>> data_to_register = {
      {"id1", {"id1", "tag1a", "tag1b"}}, {"xyz", {"xyz"}}};
  std::vector<Data> data = CreateTestData(data_to_register);
  EXPECT_EQ(data.size(), 2u);

  index_.AddOrUpdate(data);
  EXPECT_EQ(index_.GetSize(), 2u);

  // Find result with query "id1". It returns an exact match.
  FindAndCheck(&index_, "id1",
               /*max_results=*/-1, ResponseStatus::kSuccess, {"id1"});
  FindAndCheck(&index_, "abc",
               /*max_results=*/-1, ResponseStatus::kSuccess, {});
}

TEST_F(IndexTest, SearchTagSplit) {
  const std::map<std::string, std::vector<std::string>> data_to_register = {
      {"id1", {"hello hello again world!", "tag1a"}}, {"id2", {"tag2a"}}};
  std::vector<Data> data = CreateTestData(data_to_register);
  EXPECT_EQ(data.size(), 2u);

  index_.AddOrUpdate(data);
  EXPECT_EQ(index_.GetSize(), 2u);

  {
    std::vector<base::string16> search_tags;
    std::vector<base::string16> individual_search_tags;
    index_.GetSearchTagsForTesting("id1", &search_tags,
                                   &individual_search_tags);
    EXPECT_EQ(search_tags.size(), 2u);
    EXPECT_EQ(search_tags[0], base::UTF8ToUTF16("hello hello again world!"));
    EXPECT_EQ(search_tags[1], base::UTF8ToUTF16("tag1a"));
    EXPECT_EQ(individual_search_tags.size(), 4u);
    EXPECT_EQ(individual_search_tags[0], base::UTF8ToUTF16("again"));
    EXPECT_EQ(individual_search_tags[1], base::UTF8ToUTF16("hello"));
    EXPECT_EQ(individual_search_tags[2], base::UTF8ToUTF16("tag1a"));
    EXPECT_EQ(individual_search_tags[3], base::UTF8ToUTF16("world!"));
  }
  {
    std::vector<base::string16> search_tags;
    std::vector<base::string16> individual_search_tags;
    index_.GetSearchTagsForTesting("id2", &search_tags,
                                   &individual_search_tags);
    EXPECT_EQ(search_tags.size(), 1u);
    EXPECT_EQ(search_tags[0], base::UTF8ToUTF16("tag2a"));
    EXPECT_EQ(individual_search_tags.size(), 0u);
  }

  // Single-word query.
  FindAndCheck(&index_, "vorld", /*max_results=*/-1, ResponseStatus::kSuccess,
               {"id1"});
  // Multi-word query.
  FindAndCheck(&index_, "hello vorld hello", /*max_results=*/-1,
               ResponseStatus::kSuccess, {"id1"});

  EXPECT_EQ(index_.Delete({"id1", "id10"}), 1u);
  {
    std::vector<base::string16> search_tags;
    std::vector<base::string16> individual_search_tags;
    index_.GetSearchTagsForTesting("id1", &search_tags,
                                   &individual_search_tags);
    EXPECT_EQ(search_tags.size(), 0u);
    EXPECT_EQ(individual_search_tags.size(), 0u);
  }
}

TEST_F(IndexTest, SearchTagNotSplit) {
  const std::map<std::string, std::vector<std::string>> data_to_register = {
      {"id1", {"hello hello again world!", "tag1a"}}, {"id2", {"tag2a"}}};
  std::vector<Data> data = CreateTestData(data_to_register);
  EXPECT_EQ(data.size(), 2u);

  SearchParams search_params;
  search_params.split_search_tags = false;
  index_.SetSearchParams(search_params);
  index_.AddOrUpdate(data);
  EXPECT_EQ(index_.GetSize(), 2u);

  {
    std::vector<base::string16> search_tags;
    std::vector<base::string16> individual_search_tags;
    index_.GetSearchTagsForTesting("id1", &search_tags,
                                   &individual_search_tags);
    EXPECT_EQ(search_tags.size(), 2u);
    EXPECT_EQ(search_tags[0], base::UTF8ToUTF16("hello hello again world!"));
    EXPECT_EQ(search_tags[1], base::UTF8ToUTF16("tag1a"));
    EXPECT_EQ(individual_search_tags.size(), 0u);
  }
  {
    std::vector<base::string16> search_tags;
    std::vector<base::string16> individual_search_tags;
    index_.GetSearchTagsForTesting("id2", &search_tags,
                                   &individual_search_tags);
    EXPECT_EQ(search_tags.size(), 1u);
    EXPECT_EQ(search_tags[0], base::UTF8ToUTF16("tag2a"));
    EXPECT_EQ(individual_search_tags.size(), 0u);
  }

  // Single-word query.
  FindAndCheck(&index_, "vorld", /*max_results=*/-1, ResponseStatus::kSuccess,
               {"id1"});
  // Multi-word query.
  FindAndCheck(&index_, "hello vorld hello", /*max_results=*/-1,
               ResponseStatus::kSuccess, {"id1"});

  EXPECT_EQ(index_.Delete({"id1", "id10"}), 1u);
  {
    std::vector<base::string16> search_tags;
    std::vector<base::string16> individual_search_tags;
    index_.GetSearchTagsForTesting("id1", &search_tags,
                                   &individual_search_tags);
    EXPECT_EQ(search_tags.size(), 0u);
    EXPECT_EQ(individual_search_tags.size(), 0u);
  }
}

}  // namespace local_search_service
