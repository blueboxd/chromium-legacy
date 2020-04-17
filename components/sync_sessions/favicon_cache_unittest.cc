// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/favicon_cache.h"

#include <memory>
#include <string>
#include <utility>

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/sync/model/sync_change_processor_wrapper_for_test.h"
#include "components/sync/model/sync_error_factory_mock.h"
#include "components/sync/model/time.h"
#include "components/sync/protocol/favicon_image_specifics.pb.h"
#include "components/sync/protocol/favicon_tracking_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace sync_sessions {

namespace {

// Maximum number of favicon mappings to keep.
const int kMaxFaviconMappings = 4;

// TestFaviconData ------------------------------------------------------------
struct TestFaviconData {
  TestFaviconData() : is_bookmarked(false) {}
  GURL page_url;
  GURL icon_url;
  std::string image_16;
  std::string image_32;
  std::string image_64;
  bool is_bookmarked;
};

GURL GetFaviconPageUrl(int index) {
  return GURL(base::StringPrintf("http://bla.com/%.2i.html", index));
}

GURL GetFaviconIconUrl(int index) {
  return GURL(base::StringPrintf("http://bla.com/%.2i.ico", index));
}

GURL GetUpdatedFaviconIconUrl(int index) {
  return GURL(base::StringPrintf("http://bla.com/moved/%.2i.ico", index));
}

TestFaviconData BuildFaviconData(int index) {
  TestFaviconData data;
  data.page_url = GetFaviconPageUrl(index);
  data.icon_url = GetFaviconIconUrl(index);
  data.image_16 = base::StringPrintf("16 %i", index);
  // TODO(zea): enable this once the cache supports writing them.
  // data.image_32 = base::StringPrintf("32 %i", index);
  // data.image_64 = base::StringPrintf("64 %i", index);
  return data;
}

}  // namespace

class SyncFaviconCacheTest : public testing::Test {
 public:
  SyncFaviconCacheTest();
  ~SyncFaviconCacheTest() override {}

  size_t GetTaskCount() const;
  size_t GetFaviconCount() const;

  FaviconCache* cache() { return &cache_; }

  // Populates the local cache of favicons in FaviconService with the data
  // described in |test_data|.
  void PopulateFaviconService(const TestFaviconData& test_data);

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  testing::NiceMock<favicon::MockFaviconService> mock_favicon_service_;
  FaviconCache cache_;
};

SyncFaviconCacheTest::SyncFaviconCacheTest()
    : task_environment_(
          base::test::SingleThreadTaskEnvironment::MainThreadType::UI),
      cache_(&mock_favicon_service_, nullptr, kMaxFaviconMappings) {}

size_t SyncFaviconCacheTest::GetTaskCount() const {
  return cache_.NumTasksForTest();
}

size_t SyncFaviconCacheTest::GetFaviconCount() const {
  return cache_.NumFaviconMappingsForTest();
}

void SyncFaviconCacheTest::PopulateFaviconService(
    const TestFaviconData& test_data) {
  std::vector<favicon_base::FaviconRawBitmapResult> bitmap_results;
  if (!test_data.image_16.empty()) {
    favicon_base::FaviconRawBitmapResult bitmap_result;
    bitmap_result.icon_url = test_data.icon_url;
    bitmap_result.pixel_size.set_width(16);
    bitmap_result.pixel_size.set_height(16);
    base::RefCountedString* temp_string = new base::RefCountedString();
    temp_string->data() = test_data.image_16;
    bitmap_result.bitmap_data = temp_string;
    bitmap_results.push_back(bitmap_result);
  }
  if (!test_data.image_32.empty()) {
    favicon_base::FaviconRawBitmapResult bitmap_result;
    bitmap_result.icon_url = test_data.icon_url;
    bitmap_result.pixel_size.set_width(32);
    bitmap_result.pixel_size.set_height(32);
    base::RefCountedString* temp_string = new base::RefCountedString();
    temp_string->data() = test_data.image_32;
    bitmap_result.bitmap_data = temp_string;
    bitmap_results.push_back(bitmap_result);
  }
  if (!test_data.image_64.empty()) {
    favicon_base::FaviconRawBitmapResult bitmap_result;
    bitmap_result.icon_url = test_data.icon_url;
    bitmap_result.pixel_size.set_width(64);
    bitmap_result.pixel_size.set_height(64);
    base::RefCountedString* temp_string = new base::RefCountedString();
    temp_string->data() = test_data.image_64;
    bitmap_result.bitmap_data = temp_string;
    bitmap_results.push_back(bitmap_result);
  }

  ON_CALL(mock_favicon_service_,
          GetFaviconForPageURL(test_data.page_url, _, _, _, _))
      .WillByDefault([=](auto, auto, auto,
                         favicon_base::FaviconResultsCallback callback,
                         base::CancelableTaskTracker* tracker) {
        return tracker->PostTask(
            base::ThreadTaskRunnerHandle::Get().get(), FROM_HERE,
            base::BindOnce(std::move(callback), bitmap_results));
      });
}

// A freshly constructed cache should be empty.
TEST_F(SyncFaviconCacheTest, Empty) {
  EXPECT_EQ(0U, GetFaviconCount());
}

// Test that visiting a new page stores the mapping.
TEST_F(SyncFaviconCacheTest, AddMapOnFaviconVisited) {
  ASSERT_EQ(0U, GetFaviconCount());
  cache()->OnFaviconVisited(GetFaviconPageUrl(0), GetFaviconIconUrl(0));

  EXPECT_EQ(0U, GetTaskCount());
  EXPECT_EQ(1U, GetFaviconCount());

  EXPECT_EQ(cache()->GetIconUrlForPageUrl(GetFaviconPageUrl(0)),
            GetFaviconIconUrl(0));
}

// Test that visiting a new page stores the mapping.
TEST_F(SyncFaviconCacheTest, UpdateMapOnFaviconVisited) {
  ASSERT_EQ(0U, GetFaviconCount());
  cache()->OnFaviconVisited(GetFaviconPageUrl(0), GetFaviconIconUrl(0));
  cache()->OnFaviconVisited(GetFaviconPageUrl(1), GetFaviconIconUrl(1));

  ASSERT_EQ(0U, GetTaskCount());
  ASSERT_EQ(2U, GetFaviconCount());

  cache()->OnFaviconVisited(GetFaviconPageUrl(0), GetUpdatedFaviconIconUrl(0));

  ASSERT_EQ(0U, GetTaskCount());
  ASSERT_EQ(2U, GetFaviconCount());

  EXPECT_EQ(cache()->GetIconUrlForPageUrl(GetFaviconPageUrl(0)),
            GetUpdatedFaviconIconUrl(0));
  EXPECT_EQ(cache()->GetIconUrlForPageUrl(GetFaviconPageUrl(1)),
            GetFaviconIconUrl(1));
}

// Test that visiting a new page triggers a favicon load if the provided icon
// url is invalid.
TEST_F(SyncFaviconCacheTest, AddMapOnFaviconVisitedWithInvalidIconUrl) {
  ASSERT_EQ(0U, GetFaviconCount());
  PopulateFaviconService(BuildFaviconData(0));
  cache()->OnFaviconVisited(GetFaviconPageUrl(0), GURL());

  EXPECT_EQ(1U, GetTaskCount());
  // Wait until the FaviconService replies with the results populated above.
  RunUntilIdle();
  EXPECT_EQ(0U, GetTaskCount());
  EXPECT_EQ(1U, GetFaviconCount());

  EXPECT_EQ(cache()->GetIconUrlForPageUrl(GetFaviconPageUrl(0)),
            GetFaviconIconUrl(0));
}

// Test that visiting a new page triggers a favicon load if the provided icon
// url is invalid. Nevertheless, any favicon urls with the "data" scheme should
// be ignored.
TEST_F(SyncFaviconCacheTest, AddMapOnFaviconVisitedIgnoreDataScheme) {
  ASSERT_EQ(0U, GetFaviconCount());
  TestFaviconData test_data = BuildFaviconData(0);
  test_data.icon_url = GURL("data:image/png;base64;blabla");
  EXPECT_TRUE(test_data.icon_url.is_valid());
  PopulateFaviconService(test_data);
  cache()->OnFaviconVisited(GetFaviconPageUrl(0), GURL());

  EXPECT_EQ(1U, GetTaskCount());
  // Wait until the FaviconService replies with the results populated above.
  RunUntilIdle();
  EXPECT_EQ(0U, GetTaskCount());
  EXPECT_EQ(0U, GetFaviconCount());
}

// Test that the cache has a max limit on mappings.
TEST_F(SyncFaviconCacheTest, AddMapOnFaviconVisitedMaxLimit) {
  ASSERT_EQ(0U, GetFaviconCount());
  for (int i = 0; i < kMaxFaviconMappings; ++i) {
    cache()->OnFaviconVisited(GetFaviconPageUrl(i), GetFaviconIconUrl(i));
  }
  ASSERT_EQ(static_cast<size_t>(kMaxFaviconMappings), GetFaviconCount());

  // Adding one more mapping deletes the first added mapping.
  cache()->OnFaviconVisited(GetFaviconPageUrl(kMaxFaviconMappings),
                            GetFaviconIconUrl(kMaxFaviconMappings));
  ASSERT_EQ(static_cast<size_t>(kMaxFaviconMappings), GetFaviconCount());
  EXPECT_EQ(cache()->GetIconUrlForPageUrl(GetFaviconPageUrl(0)), GURL());
  for (int i = 1; i < kMaxFaviconMappings + 1; ++i) {
    EXPECT_EQ(cache()->GetIconUrlForPageUrl(GetFaviconPageUrl(i)),
              GetFaviconIconUrl(i));
  }
}

TEST_F(SyncFaviconCacheTest, UpdateMappingsFromForeignTab) {
  ASSERT_EQ(0U, GetFaviconCount());
  cache()->OnFaviconVisited(GetFaviconPageUrl(0), GetFaviconIconUrl(0));
  cache()->OnFaviconVisited(GetFaviconPageUrl(1), GetFaviconIconUrl(1));

  ASSERT_EQ(0U, GetTaskCount());
  ASSERT_EQ(2U, GetFaviconCount());

  // Feed in the mapping.
  sync_pb::SessionTab tab;
  sync_pb::TabNavigation* navigation = tab.add_navigation();
  navigation->set_virtual_url(GetFaviconPageUrl(0).spec());
  navigation->set_favicon_url(GetUpdatedFaviconIconUrl(0).spec());
  cache()->UpdateMappingsFromForeignTab(tab);

  ASSERT_EQ(0U, GetTaskCount());
  ASSERT_EQ(2U, GetFaviconCount());

  EXPECT_EQ(cache()->GetIconUrlForPageUrl(GetFaviconPageUrl(0)),
            GetUpdatedFaviconIconUrl(0));
  EXPECT_EQ(cache()->GetIconUrlForPageUrl(GetFaviconPageUrl(1)),
            GetFaviconIconUrl(1));
}

TEST_F(SyncFaviconCacheTest, UpdateMappingsFromForeignTabIgnoresDataScheme) {
  ASSERT_EQ(0U, GetFaviconCount());
  // Feed in the mapping.
  sync_pb::SessionTab tab;
  sync_pb::TabNavigation* navigation = tab.add_navigation();
  navigation->set_virtual_url(GetFaviconPageUrl(0).spec());
  navigation->set_favicon_url("data:image/png;base64;blabla");
  cache()->UpdateMappingsFromForeignTab(tab);

  EXPECT_EQ(0U, GetTaskCount());
  EXPECT_EQ(0U, GetFaviconCount());
}

// Test that visiting a new page triggers a favicon load if the provided icon
// url is invalid.
TEST_F(SyncFaviconCacheTest, OnPageFaviconUpdatedLooksUpFavicons) {
  ASSERT_EQ(0U, GetFaviconCount());
  PopulateFaviconService(BuildFaviconData(0));
  cache()->OnPageFaviconUpdated(GetFaviconPageUrl(0));

  EXPECT_EQ(1U, GetTaskCount());
  // Wait until the FaviconService replies with the results populated above.
  RunUntilIdle();
  EXPECT_EQ(0U, GetTaskCount());
  EXPECT_EQ(1U, GetFaviconCount());

  EXPECT_EQ(cache()->GetIconUrlForPageUrl(GetFaviconPageUrl(0)),
            GetFaviconIconUrl(0));
}

// Test that visiting a new page triggers a favicon load if the provided icon
// url is invalid.
TEST_F(SyncFaviconCacheTest, OnPageFaviconUpdatedIgnoredIfMappingExists) {
  ASSERT_EQ(0U, GetFaviconCount());
  cache()->OnFaviconVisited(GetFaviconPageUrl(0), GetFaviconIconUrl(0));
  ASSERT_EQ(0U, GetTaskCount());
  ASSERT_EQ(1U, GetFaviconCount());

  TestFaviconData test_data = BuildFaviconData(0);
  test_data.icon_url = GetUpdatedFaviconIconUrl(0);
  PopulateFaviconService(test_data);
  cache()->OnPageFaviconUpdated(GetFaviconPageUrl(0));
  EXPECT_EQ(0U, GetTaskCount());

  // This is ignored, the old mapping stays around.
  EXPECT_EQ(cache()->GetIconUrlForPageUrl(GetFaviconPageUrl(0)),
            GetFaviconIconUrl(0));
}

// A full history clear notification should result in all mappings being
// deleted.
TEST_F(SyncFaviconCacheTest, HistoryFullClear) {
  ASSERT_EQ(0U, GetFaviconCount());
  cache()->OnFaviconVisited(GetFaviconPageUrl(0), GetFaviconIconUrl(0));
  cache()->OnFaviconVisited(GetFaviconPageUrl(1), GetFaviconIconUrl(1));
  cache()->OnFaviconVisited(GetFaviconPageUrl(2), GetFaviconIconUrl(2));
  ASSERT_EQ(3U, GetFaviconCount());

  cache()->OnURLsDeleted(nullptr, history::DeletionInfo::ForAllHistory());

  EXPECT_EQ(0U, GetFaviconCount());
}

// A partial history clear notification should result in the expired favicons
// also being deleted from the map.
TEST_F(SyncFaviconCacheTest, HistorySubsetClear) {
  ASSERT_EQ(0U, GetFaviconCount());
  cache()->OnFaviconVisited(GetFaviconPageUrl(0), GetFaviconIconUrl(0));
  cache()->OnFaviconVisited(GetFaviconPageUrl(1), GetFaviconIconUrl(1));
  cache()->OnFaviconVisited(GetFaviconPageUrl(2), GetFaviconIconUrl(2));
  cache()->OnFaviconVisited(GetFaviconPageUrl(3), GetFaviconIconUrl(3));
  ASSERT_EQ(4U, GetFaviconCount());

  std::set<GURL> favicon_urls_to_delete;
  favicon_urls_to_delete.insert(GetFaviconIconUrl(2));
  favicon_urls_to_delete.insert(GetFaviconIconUrl(3));
  cache()->OnURLsDeleted(
      nullptr, history::DeletionInfo::ForUrls(history::URLRows(),
                                              favicon_urls_to_delete));

  EXPECT_EQ(2U, GetFaviconCount());
  EXPECT_EQ(cache()->GetIconUrlForPageUrl(GetFaviconPageUrl(0)),
            GetFaviconIconUrl(0));
  EXPECT_EQ(cache()->GetIconUrlForPageUrl(GetFaviconPageUrl(1)),
            GetFaviconIconUrl(1));
  EXPECT_EQ(cache()->GetIconUrlForPageUrl(GetFaviconPageUrl(2)), GURL());
  EXPECT_EQ(cache()->GetIconUrlForPageUrl(GetFaviconPageUrl(3)), GURL());
}

}  // namespace sync_sessions
