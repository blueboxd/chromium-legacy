// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chrome/services/local_search_service/local_search_service_impl.h"
#include "chrome/services/local_search_service/public/mojom/types.mojom.h"
#include "chrome/services/local_search_service/test_utils.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace local_search_service {

class LocalSearchServiceImplTest : public testing::Test {
 public:
  LocalSearchServiceImplTest() {
    service_impl_.BindReceiver(service_remote_.BindNewPipeAndPassReceiver());
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  LocalSearchServiceImpl service_impl_;
  mojo::Remote<mojom::LocalSearchService> service_remote_;
};

// Tests a query that results in an exact match. We do not aim to test the
// algorithm used in the search, but exact match should always be returned.
TEST_F(LocalSearchServiceImplTest, ResultFound) {
  mojo::Remote<mojom::Index> index_remote;
  service_remote_->GetIndex(mojom::LocalSearchService::IndexId::CROS_SETTINGS,
                            index_remote.BindNewPipeAndPassReceiver());

  GetSizeAndCheck(index_remote.get(), 0u);

  // Register the following data to the search index, the map is id to
  // search-tags.
  const std::map<std::string, std::vector<std::string>> data_to_register = {
      {"id1", {"id1", "tag1a", "tag1b"}}, {"xyz", {"xyz"}}};
  std::vector<mojom::DataPtr> data = CreateTestData(data_to_register);
  EXPECT_EQ(data.size(), 2u);

  AddOrUpdateAndCheck(index_remote.get(), std::move(data));
  GetSizeAndCheck(index_remote.get(), 2u);

  // Find result with query "id1". It returns an exact match.
  FindAndCheck(index_remote.get(), "id1", /*max_latency_in_ms=*/-1,
               /*max_results=*/-1, mojom::ResponseStatus::SUCCESS, {"id1"});
}

// Tests a query that results in no match. We do not aim to test the algorithm
// used in the search, but a query too different from the item should have no
// result returned.
TEST_F(LocalSearchServiceImplTest, ResultNotFound) {
  mojo::Remote<mojom::Index> index_remote;
  service_remote_->GetIndex(mojom::LocalSearchService::IndexId::CROS_SETTINGS,
                            index_remote.BindNewPipeAndPassReceiver());

  GetSizeAndCheck(index_remote.get(), 0u);

  // Register the following data to the search index, the map is id to
  // search-tags.
  const std::map<std::string, std::vector<std::string>> data_to_register = {
      {"id1", {"id1", "tag1a", "tag1b"}}, {"id2", {"id2", "tag2a", "tag2b"}}};
  std::vector<mojom::DataPtr> data = CreateTestData(data_to_register);
  EXPECT_EQ(data.size(), 2u);

  AddOrUpdateAndCheck(index_remote.get(), std::move(data));
  GetSizeAndCheck(index_remote.get(), 2u);

  // Find result with query "xyz". It returns no match.
  FindAndCheck(index_remote.get(), "xyz", /*max_latency_in_ms=*/-1,
               /*max_results=*/-1, mojom::ResponseStatus::SUCCESS, {});
}

TEST_F(LocalSearchServiceImplTest, UpdateData) {
  mojo::Remote<mojom::Index> index_remote;
  service_remote_->GetIndex(mojom::LocalSearchService::IndexId::CROS_SETTINGS,
                            index_remote.BindNewPipeAndPassReceiver());

  GetSizeAndCheck(index_remote.get(), 0u);

  // Register the following data to the search index, the map is id to
  // search-tags.
  const std::map<std::string, std::vector<std::string>> data_to_register = {
      {"id1", {"tag1a", "tag1b"}}, {"id2", {"tag2a", "tag2b"}}};
  std::vector<mojom::DataPtr> data = CreateTestData(data_to_register);
  EXPECT_EQ(data.size(), 2u);

  AddOrUpdateAndCheck(index_remote.get(), std::move(data));
  GetSizeAndCheck(index_remote.get(), 2u);

  // Delete "id1" and "id10" from the index. Since "id10" doesn't exist, only
  // one item is deleted.
  DeleteAndCheck(index_remote.get(), {"id1", "id10"}, 1u);
  GetSizeAndCheck(index_remote.get(), 1u);

  // Add "id3" to the index.
  mojom::DataPtr data_id3 = mojom::Data::New(
      "id3", std::vector<base::string16>({base::UTF8ToUTF16("tag3a")}));
  std::vector<mojom::DataPtr> data_to_update;
  data_to_update.push_back(std::move(data_id3));
  AddOrUpdateAndCheck(index_remote.get(), std::move(data_to_update));
  GetSizeAndCheck(index_remote.get(), 2u);

  FindAndCheck(index_remote.get(), "id3", /*max_latency_in_ms=*/-1,
               /*max_results=*/-1, mojom::ResponseStatus::SUCCESS, {"id3"});
  FindAndCheck(index_remote.get(), "id1", /*max_latency_in_ms=*/-1,
               /*max_results=*/-1, mojom::ResponseStatus::SUCCESS, {});
}
}  // namespace local_search_service
