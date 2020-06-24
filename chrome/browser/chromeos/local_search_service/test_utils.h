// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_TEST_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_TEST_UTILS_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/local_search_service/shared_structs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace local_search_service {

// Creates test data to be registered to the index. |input| is a map from
// id to contents (id and content).
std::vector<Data> CreateTestData(
    const std::map<std::string,
                   std::vector<std::pair<std::string, std::string>>>& input);

}  // namespace local_search_service

#endif  // CHROME_BROWSER_CHROMEOS_LOCAL_SEARCH_SERVICE_TEST_UTILS_H_
