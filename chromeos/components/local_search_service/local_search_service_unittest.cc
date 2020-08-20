// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/test/task_environment.h"
#include "chromeos/components/local_search_service/index.h"
#include "chromeos/components/local_search_service/local_search_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace local_search_service {

class LocalSearchServiceTest : public testing::Test {
 protected:
  LocalSearchService service_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::DEFAULT,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
};

TEST_F(LocalSearchServiceTest, GetLinearMapSearch) {
  Index* const index = service_.GetIndex(
      IndexId::kCrosSettings, Backend::kLinearMap, nullptr /* local_state */);
  CHECK(index);

  EXPECT_EQ(index->GetSize(), 0u);
}

TEST_F(LocalSearchServiceTest, GetInvertedIndexSearch) {
  Index* const index =
      service_.GetIndex(IndexId::kCrosSettings, Backend::kInvertedIndex,
                        nullptr /* local_state */);
  CHECK(index);

  EXPECT_EQ(index->GetSize(), 0u);
}

}  // namespace local_search_service
}  // namespace chromeos
