// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_index/content_index_provider_impl.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/time/time.h"
#include "chrome/test/base/testing_profile.h"
#include "components/offline_items_collection/core/offline_content_provider.h"
#include "content/public/browser/content_index_provider.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_storage_partition.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"
#include "url/origin.h"

using offline_items_collection::ContentId;
using offline_items_collection::OfflineContentAggregator;
using offline_items_collection::OfflineContentProvider;
using offline_items_collection::OfflineItem;
using offline_items_collection::OfflineItemVisuals;
using offline_items_collection::UpdateDelta;
using testing::_;

constexpr int64_t kServiceWorkerRegistrationId = 42;
const GURL kLaunchURL = GURL("https://example.com/foo");
const url::Origin kOrigin = url::Origin::Create(kLaunchURL.GetOrigin());

class ContentIndexProviderImplTest : public testing::Test,
                                     public OfflineContentProvider::Observer {
 public:
  void SetUp() override {
    provider_ = std::make_unique<ContentIndexProviderImpl>(&profile_);
    provider_->AddObserver(this);
  }

  void TearDown() override { provider_->RemoveObserver(this); }

  // OfflineContentProvider::Observer implementation.
  MOCK_METHOD1(OnItemsAdded,
               void(const OfflineContentProvider::OfflineItemList& items));
  MOCK_METHOD1(OnItemRemoved, void(const ContentId& id));
  MOCK_METHOD2(OnItemUpdated,
               void(const OfflineItem& item,
                    const base::Optional<UpdateDelta>& update_delta));

  content::ContentIndexEntry CreateEntry(const std::string& id) {
    auto description = blink::mojom::ContentDescription::New(
        id, "title", "description", blink::mojom::ContentCategory::ARTICLE,
        "icon_url", "launch_url");
    return content::ContentIndexEntry(kServiceWorkerRegistrationId,
                                      std::move(description), kLaunchURL,
                                      base::Time::Now());
  }

  std::vector<OfflineItem> GetAllItems() {
    std::vector<OfflineItem> out_items;

    base::RunLoop run_loop;
    provider_->GetAllItems(
        base::BindLambdaForTesting([&](const std::vector<OfflineItem>& items) {
          out_items = items;
          run_loop.Quit();
        }));
    run_loop.Run();

    return out_items;
  }

 protected:
  content::TestBrowserThreadBundle threads_;
  TestingProfile profile_;
  std::unique_ptr<ContentIndexProviderImpl> provider_;
};

TEST_F(ContentIndexProviderImplTest, OfflineItemCreation) {
  std::vector<OfflineItem> items;
  {
    EXPECT_CALL(*this, OnItemsAdded(_)).WillOnce(testing::SaveArg<0>(&items));
    provider_->OnContentAdded(CreateEntry("id"));
  }
  ASSERT_EQ(items.size(), 1u);
  const auto& item = items[0];

  EXPECT_FALSE(item.id.name_space.empty());
  EXPECT_FALSE(item.id.id.empty());
  EXPECT_FALSE(item.title.empty());
  EXPECT_FALSE(item.description.empty());
  EXPECT_FALSE(item.is_transient);
  EXPECT_TRUE(item.is_openable);
  EXPECT_EQ(item.page_url, kLaunchURL);
}

TEST_F(ContentIndexProviderImplTest, ObserverUpdates) {
  {
    EXPECT_CALL(*this, OnItemsAdded(_));
    provider_->OnContentAdded(CreateEntry("id"));
  }

  // Adding an already existing ID should call update.
  {
    EXPECT_CALL(*this, OnItemsAdded(_)).Times(0);
    EXPECT_CALL(*this, OnItemUpdated(_, _));
    provider_->OnContentAdded(CreateEntry("id"));
  }

  // Removing a fake ID won't notify observers.
  {
    EXPECT_CALL(*this, OnItemRemoved(_)).Times(0);
    provider_->OnContentDeleted(kServiceWorkerRegistrationId, kOrigin,
                                "wrong-id");
  }

  {
    EXPECT_CALL(*this, OnItemRemoved(_));
    provider_->OnContentDeleted(kServiceWorkerRegistrationId, kOrigin, "id");
  }
}

TEST_F(ContentIndexProviderImplTest, GetAllItems) {
  // Inititally there are no items.
  EXPECT_TRUE(GetAllItems().empty());

  provider_->OnContentAdded(CreateEntry("id1"));
  provider_->OnContentAdded(CreateEntry("id2"));

  auto items = GetAllItems();
  ASSERT_EQ(items.size(), 2u);
  EXPECT_EQ(items[0].id.name_space, items[0].id.name_space);
  EXPECT_NE(items[0].id.id, items[1].id.id);
}
