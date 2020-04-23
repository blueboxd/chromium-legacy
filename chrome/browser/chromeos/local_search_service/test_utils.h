// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_TEST_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_TEST_UTILS_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "chrome/browser/chromeos/local_search_service/index.h"

namespace local_search_service {

// Creates test data to be registered to the index. |input| is a map from
// id to search-tags.
std::vector<Data> CreateTestData(
    const std::map<std::string, std::vector<std::string>>& input);

// Finds item for |query| from |index| and checks their ids are those in
// |expected_result_ids|.
void FindAndCheck(Index* index,
                  std::string query,
                  int32_t max_results,
                  ResponseStatus expected_status,
                  const std::vector<std::string>& expected_result_ids);
}  // namespace local_search_service

#endif  // CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_TEST_UTILS_H_
