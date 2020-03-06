// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/local_search_service/test_utils.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/services/local_search_service/public/mojom/local_search_service.mojom-test-utils.h"
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

std::vector<mojom::DataPtr> CreateTestData(
    const std::map<std::string, std::vector<std::string>>& input) {
  std::vector<mojom::DataPtr> output;
  for (const auto& item : input) {
    const std::vector<base::string16> tags = MultiUTF8ToUTF16(item.second);
    mojom::DataPtr data = mojom::Data::New(item.first, tags);
    output.push_back(std::move(data));
  }
  return output;
}

void GetSizeAndCheck(mojom::Index* index, uint64_t expected_num_items) {
  DCHECK(index);
  uint64_t num_items = 0;
  mojom::IndexAsyncWaiter(index).GetSize(&num_items);
  EXPECT_EQ(num_items, expected_num_items);
}

void AddOrUpdateAndCheck(mojom::Index* index,
                         std::vector<mojom::DataPtr> data) {
  DCHECK(index);
  mojom::IndexAsyncWaiter(index).AddOrUpdate(std::move(data));
}

void DeleteAndCheck(mojom::Index* index,
                    const std::vector<std::string>& ids,
                    uint32_t expected_num_deleted) {
  DCHECK(index);
  uint32_t num_deleted = 0u;
  mojom::IndexAsyncWaiter(index).Delete(ids, &num_deleted);
  EXPECT_EQ(num_deleted, expected_num_deleted);
}

void FindAndCheck(mojom::Index* index,
                  std::string query,
                  int32_t max_latency_in_ms,
                  int32_t max_results,
                  mojom::ResponseStatus expected_status,
                  const std::vector<std::string>& expected_result_ids) {
  DCHECK(index);

  mojom::IndexAsyncWaiter async_waiter(index);
  mojom::ResponseStatus status = mojom::ResponseStatus::UNKNOWN_ERROR;
  base::Optional<std::vector<::local_search_service::mojom::ResultPtr>> results;
  async_waiter.Find(base::UTF8ToUTF16(query), max_latency_in_ms, max_results,
                    &status, &results);

  EXPECT_EQ(status, expected_status);

  if (results) {
    // If results are returned, check size and values match the expected.
    EXPECT_EQ(results->size(), expected_result_ids.size());
    for (size_t i = 0; i < results->size(); ++i) {
      EXPECT_EQ((*results)[i]->id, expected_result_ids[i]);
      // Scores should be non-increasing.
      if (i < results->size() - 1) {
        EXPECT_GE((*results)[i]->score, (*results)[i + 1]->score);
      }
    }
    return;
  }

  // If no results are returned, expected ids should be empty.
  EXPECT_TRUE(expected_result_ids.empty());
}

}  // namespace local_search_service
