// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "chrome/browser/local_search_service/local_search_service_proxy.h"
#include "chrome/services/local_search_service/index_impl.h"
#include "chrome/services/local_search_service/local_search_service_impl.h"
#include "chrome/services/local_search_service/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace local_search_service {

class LocalSearchServiceProxyTest : public testing::Test {};

TEST_F(LocalSearchServiceProxyTest, Basic) {
  LocalSearchServiceProxy service_proxy(nullptr);
  LocalSearchServiceImpl* const service =
      service_proxy.GetLocalSearchServiceImpl();
  DCHECK(service);

  IndexImpl* const index = service->GetIndexImpl(IndexId::kCrosSettings);
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
}

}  // namespace local_search_service
