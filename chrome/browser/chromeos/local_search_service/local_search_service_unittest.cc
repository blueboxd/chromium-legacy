// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/local_search_service/local_search_service.h"
#include "chrome/browser/chromeos/local_search_service/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace local_search_service {

class LocalSearchServiceTest : public testing::Test {
 protected:
  LocalSearchService service_;
};

// Tests a query that results in an exact match. We do not aim to test the
// algorithm used in the search, but exact match should always be returned.
TEST_F(LocalSearchServiceTest, ResultFound) {
  Index* const index = service_.GetIndex(IndexId::kCrosSettings);
  DCHECK(index);

  EXPECT_EQ(index->GetSize(), 0u);

  // Register the following data to the search index, the map is id to
  // search-tags.
  const std::map<std::string, std::vector<std::string>> data_to_register = {
      {"id1", {"id1", "tag1a", "tag1b"}}, {"xyz", {"xyz"}}};
  std::vector<Data> data = CreateTestData(data_to_register);
  EXPECT_EQ(data.size(), 2u);

  index->AddOrUpdate(data);
  EXPECT_EQ(index->GetSize(), 2u);

  // Find result with query "id1". It returns an exact match.
  FindAndCheck(index, "id1",
               /*max_results=*/-1, ResponseStatus::kSuccess, {"id1"});
}

// Tests a query that results in no match. We do not aim to test the algorithm
// used in the search, but a query too different from the item should have no
// result returned.
TEST_F(LocalSearchServiceTest, ResultNotFound) {
  Index* const index = service_.GetIndex(IndexId::kCrosSettings);
  DCHECK(index);

  EXPECT_EQ(index->GetSize(), 0u);

  // Register the following data to the search index, the map is id to
  // search-tags.
  const std::map<std::string, std::vector<std::string>> data_to_register = {
      {"id1", {"id1", "tag1a", "tag1b"}}, {"id2", {"id2", "tag2a", "tag2b"}}};
  std::vector<Data> data = CreateTestData(data_to_register);
  EXPECT_EQ(data.size(), 2u);

  index->AddOrUpdate(data);
  EXPECT_EQ(index->GetSize(), 2u);

  // Find result with query "xyz". It returns no match.
  FindAndCheck(index, "xyz",
               /*max_results=*/-1, ResponseStatus::kSuccess, {});
}

TEST_F(LocalSearchServiceTest, UpdateData) {
  Index* const index = service_.GetIndex(IndexId::kCrosSettings);
  DCHECK(index);

  EXPECT_EQ(index->GetSize(), 0u);

  // Register the following data to the search index, the map is id to
  // search-tags.
  const std::map<std::string, std::vector<std::string>> data_to_register = {
      {"id1", {"tag1a", "tag1b"}}, {"id2", {"tag2a", "tag2b"}}};
  std::vector<Data> data = CreateTestData(data_to_register);
  EXPECT_EQ(data.size(), 2u);

  index->AddOrUpdate(data);
  EXPECT_EQ(index->GetSize(), 2u);

  // Delete "id1" and "id10" from the index. Since "id10" doesn't exist, only
  // one item is deleted.
  EXPECT_EQ(index->Delete({"id1", "id10"}), 1u);
  EXPECT_EQ(index->GetSize(), 1u);

  // Add "id3" to the index.
  Data data_id3("id3",
                std::vector<base::string16>({base::UTF8ToUTF16("tag3a")}));
  std::vector<Data> data_to_update;
  data_to_update.push_back(data_id3);
  index->AddOrUpdate(data_to_update);
  EXPECT_EQ(index->GetSize(), 2u);

  FindAndCheck(index, "id3",
               /*max_results=*/-1, ResponseStatus::kSuccess, {"id3"});
  FindAndCheck(index, "id1",
               /*max_results=*/-1, ResponseStatus::kSuccess, {});
}
}  // namespace local_search_service
