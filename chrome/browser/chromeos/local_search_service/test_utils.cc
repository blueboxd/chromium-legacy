// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/local_search_service/test_utils.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace local_search_service {

namespace {

std::vector<base::string16> MultiUTF8ToUTF16(
    const std::vector<std::string>& input) {
  std::vector<base::string16> output;
  for (const auto& str : input) {
    output.push_back(base::UTF8ToUTF16(str));
  }
  return output;
}

}  // namespace

std::vector<Data> CreateTestData(
    const std::map<std::string, std::vector<std::string>>& input) {
  std::vector<Data> output;
  for (const auto& item : input) {
    const std::vector<base::string16> tags = MultiUTF8ToUTF16(item.second);
    const Data data(item.first, tags);
    output.push_back(data);
  }
  return output;
}

void FindAndCheck(Index* index,
                  std::string query,
                  int32_t max_results,
                  ResponseStatus expected_status,
                  const std::vector<std::string>& expected_result_ids) {
  DCHECK(index);

  std::vector<Result> results;
  auto status = index->Find(base::UTF8ToUTF16(query), max_results, &results);

  EXPECT_EQ(status, expected_status);

  if (!results.empty()) {
    // If results are returned, check size and values match the expected.
    EXPECT_EQ(results.size(), expected_result_ids.size());
    for (size_t i = 0; i < results.size(); ++i) {
      EXPECT_EQ(results[i].id, expected_result_ids[i]);
      // Scores should be non-increasing.
      if (i < results.size() - 1) {
        EXPECT_GE(results[i].score, results[i + 1].score);
      }
    }
    return;
  }

  // If no results are returned, expected ids should be empty.
  EXPECT_TRUE(expected_result_ids.empty());
}

}  // namespace local_search_service
