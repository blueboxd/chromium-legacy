// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <sstream>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/services/local_search_service/local_search_service_impl.h"
#include "chrome/services/local_search_service/public/mojom/local_search_service.mojom-test-utils.h"
#include "chrome/services/local_search_service/public/mojom/types.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace local_search_service {

namespace {
// The following helper functions call the async functions and check results.
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
  async_waiter.Find(query, max_latency_in_ms, max_results, &status, &results);

  EXPECT_EQ(status, expected_status);

  if (results) {
    // If results are returned, check size and values match the expected.
    EXPECT_EQ(results->size(), expected_result_ids.size());
    for (size_t i = 0; i < results->size(); ++i) {
      EXPECT_EQ((*results)[i]->id, expected_result_ids[i]);
    }
    return;
  }

  // If no results are returned, expected ids should be empty.
  EXPECT_TRUE(expected_result_ids.empty());
}
}  // namespace

class LocalSearchServiceImplTest : public testing::Test {
 public:
  LocalSearchServiceImplTest() {
    service_impl_.BindReceiver(service_remote_.BindNewPipeAndPassReceiver());
  }

  // Creates test data to be registered to the index. |input| is a map from
  // id to search-tags.
  std::vector<mojom::DataPtr> CreateTestData(
      const std::map<std::string, std::vector<std::string>>& input) {
    std::vector<mojom::DataPtr> output;
    for (const auto& item : input) {
      const std::string& id = item.first;
      const std::vector<std::string>& tags = item.second;
      mojom::DataPtr data = mojom::Data::New(id, tags);
      output.push_back(std::move(data));
    }
    return output;
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  LocalSearchServiceImpl service_impl_;
  mojo::Remote<mojom::LocalSearchService> service_remote_;
};

TEST_F(LocalSearchServiceImplTest, ResultFound) {
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

  // Find result with query "id1". It returns an exact match.
  FindAndCheck(index_remote.get(), "id1", /*max_latency_in_ms=*/-1,
               /*max_results=*/-1, mojom::ResponseStatus::SUCCESS, {"id1"});
}

TEST_F(LocalSearchServiceImplTest, ResultNotFound) {
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

  // Find result with query "id3". It returns no match.
  FindAndCheck(index_remote.get(), "id3", /*max_latency_in_ms=*/-1,
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
  mojom::DataPtr data_id3 =
      mojom::Data::New("id3", std::vector<std::string>({"tag3a"}));
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
