// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_LOCAL_SEARCH_SERVICE_TEST_UTILS_H_
#define CHROME_SERVICES_LOCAL_SEARCH_SERVICE_TEST_UTILS_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "chrome/services/local_search_service/public/mojom/local_search_service.mojom-test-utils.h"
#include "chrome/services/local_search_service/public/mojom/types.mojom.h"

namespace local_search_service {

// Creates test data to be registered to the index. |input| is a map from
// id to search-tags.
std::vector<mojom::DataPtr> CreateTestData(
    const std::map<std::string, std::vector<std::string>>& input);

// The following helper functions call the async functions and check results.
// Gets the number of items in |index| and verifies it is |expected_num_items|.
void GetSizeAndCheck(mojom::Index* index, uint64_t expected_num_items);

// Adds items from |data| to |index|.
void AddOrUpdateAndCheck(mojom::Index* index, std::vector<mojom::DataPtr> data);

// Deletes items with |ids| from |index| and verifies number deleted is
// |expected_num_deleted|.
void DeleteAndCheck(mojom::Index* index,
                    const std::vector<std::string>& ids,
                    uint32_t expected_num_deleted);

// Finds item for |query| from |index| and checks their ids are those in
// |expected_result_ids|.
void FindAndCheck(mojom::Index* index,
                  std::string query,
                  int32_t max_latency_in_ms,
                  int32_t max_results,
                  mojom::ResponseStatus expected_status,
                  const std::vector<std::string>& expected_result_ids);
}  // namespace local_search_service

#endif  // CHROME_SERVICES_LOCAL_SEARCH_SERVICE_TEST_UTILS_H_
