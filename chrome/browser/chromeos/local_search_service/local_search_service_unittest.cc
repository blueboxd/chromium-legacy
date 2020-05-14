// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "chrome/browser/chromeos/local_search_service/index.h"
#include "chrome/browser/chromeos/local_search_service/local_search_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace local_search_service {

class LocalSearchServiceTest : public testing::Test {
 protected:
  LocalSearchService service_;
};

TEST_F(LocalSearchServiceTest, GetIndex) {
  Index* const index = service_.GetIndex(IndexId::kCrosSettings);
  CHECK(index);

  EXPECT_EQ(index->GetSize(), 0u);
}

}  // namespace local_search_service
