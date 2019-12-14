// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/test/task_environment.h"
#include "chrome/browser/local_search_service/local_search_service_proxy.h"
#include "chrome/services/local_search_service/public/mojom/types.mojom.h"
#include "chrome/services/local_search_service/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace local_search_service {

class LocalSearchServiceProxyTest : public testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(LocalSearchServiceProxyTest, Basic) {
  LocalSearchServiceProxy service_proxy(nullptr);
  mojom::LocalSearchService* const service_remote =
      service_proxy.GetLocalSearchService();
  ASSERT_TRUE(service_remote);

  mojo::Remote<mojom::Index> index_remote;
  service_remote->GetIndex(mojom::LocalSearchService::IndexId::CROS_SETTINGS,
                           index_remote.BindNewPipeAndPassReceiver());
  CHECK(index_remote.is_connected());

  GetSizeAndCheck(index_remote.get(), 0u);

  // Register the following data to the search index, the map is id to
  // search-tags.
  const std::map<std::string, std::vector<std::string>> data_to_register = {
      {"id1", {"tag1a", "tag1b"}}, {"id2", {"tag2a", "tag2b"}}};
  std::vector<mojom::DataPtr> data = CreateTestData(data_to_register);
  EXPECT_EQ(data.size(), 2u);
  AddOrUpdateAndCheck(index_remote.get(), std::move(data));
  GetSizeAndCheck(index_remote.get(), 2u);
}

}  // namespace local_search_service
