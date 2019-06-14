// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/favicon_request_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "components/favicon/core/favicon_server_fetcher_params.h"
#include "components/favicon/core/features.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"

namespace favicon {
namespace {

using testing::_;
using testing::Invoke;

const char kDummyPageUrl[] = "https://www.example.com";
const char kDummyIconUrl[] = "https://www.example.com/favicon16.png";
const int kDesiredSizeInPixel = 16;
// TODO(victorvianna): Add unit tests specific for mobile.
const FaviconRequestPlatform kDummyPlatform = FaviconRequestPlatform::kDesktop;
const SkColor kTestColor = SK_ColorRED;
base::CancelableTaskTracker::TaskId kDummyTaskId = 1;

scoped_refptr<base::RefCountedBytes> CreateTestBitmapBytes() {
  scoped_refptr<base::RefCountedBytes> data(new base::RefCountedBytes());
  SkBitmap bitmap;
  bitmap.allocN32Pixels(kDesiredSizeInPixel, kDesiredSizeInPixel);
  bitmap.eraseColor(kTestColor);
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &data->data());
  return data;
}

favicon_base::FaviconRawBitmapResult CreateTestBitmapResult() {
  favicon_base::FaviconRawBitmapResult result;
  result.bitmap_data = CreateTestBitmapBytes();
  result.icon_url = GURL(kDummyIconUrl);
  return result;
}

favicon_base::FaviconImageResult CreateTestImageResult() {
  favicon_base::FaviconImageResult result;
  result.image = gfx::Image::CreateFrom1xPNGBytes(CreateTestBitmapBytes());
  result.icon_url = GURL(kDummyIconUrl);
  return result;
}

void StoreBitmap(favicon_base::FaviconRawBitmapResult* destination,
                 const favicon_base::FaviconRawBitmapResult& result) {
  *destination = result;
}

void StoreImage(favicon_base::FaviconImageResult* destination,
                const favicon_base::FaviconImageResult& result) {
  *destination = result;
}

class MockLargeIconService : public LargeIconService {
 public:
  MockLargeIconService() = default;
  ~MockLargeIconService() override = default;

  // TODO(victorvianna): Add custom matcher to check page url when calling
  // GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache.
  MOCK_METHOD5(GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache,
               void(std::unique_ptr<FaviconServerFetcherParams> params,
                    bool may_page_url_be_private,
                    bool should_trim_page_url_path,
                    const net::NetworkTrafficAnnotationTag& traffic_annotation,
                    favicon_base::GoogleFaviconServerCallback callback));

  MOCK_METHOD5(GetLargeIconRawBitmapOrFallbackStyleForPageUrl,
               base::CancelableTaskTracker::TaskId(
                   const GURL& page_url,
                   int min_source_size_in_pixel,
                   int desired_size_in_pixel,
                   favicon_base::LargeIconCallback callback,
                   base::CancelableTaskTracker* tracker));

  MOCK_METHOD5(GetLargeIconImageOrFallbackStyleForPageUrl,
               base::CancelableTaskTracker::TaskId(
                   const GURL& page_url,
                   int min_source_size_in_pixel,
                   int desired_size_in_pixel,
                   favicon_base::LargeIconImageCallback callback,
                   base::CancelableTaskTracker* tracker));

  MOCK_METHOD5(GetLargeIconRawBitmapOrFallbackStyleForIconUrl,
               base::CancelableTaskTracker::TaskId(
                   const GURL& icon_url,
                   int min_source_size_in_pixel,
                   int desired_size_in_pixel,
                   favicon_base::LargeIconCallback callback,
                   base::CancelableTaskTracker* tracker));

  MOCK_METHOD1(TouchIconFromGoogleServer, void(const GURL& icon_url));
};

class FaviconRequestHandlerTest : public ::testing::Test {
 public:
  FaviconRequestHandlerTest()
      : favicon_request_handler_(&mock_favicon_service_,
                                 &mock_large_icon_service_) {}

 protected:
  testing::NiceMock<MockFaviconService> mock_favicon_service_;
  testing::NiceMock<MockLargeIconService> mock_large_icon_service_;
  base::MockCallback<FaviconRequestHandler::SyncedFaviconGetter>
      synced_favicon_getter_;
  base::CancelableTaskTracker tracker_;
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList scoped_feature_list_;
  FaviconRequestHandler favicon_request_handler_;
};

TEST_F(FaviconRequestHandlerTest, ShouldGetEmptyBitmap) {
  scoped_feature_list_.InitAndDisableFeature(
      kEnableHistoryFaviconsGoogleServerQuery);
  EXPECT_CALL(mock_favicon_service_,
              GetRawFaviconForPageURL(GURL(kDummyPageUrl), _,
                                      kDesiredSizeInPixel, _, _, &tracker_))
      .WillOnce([](auto, auto, auto, auto,
                   favicon_base::FaviconRawBitmapCallback callback, auto) {
        std::move(callback).Run(favicon_base::FaviconRawBitmapResult());
        return kDummyTaskId;
      });
  EXPECT_CALL(synced_favicon_getter_, Run(GURL(kDummyPageUrl)))
      .WillOnce([](auto) { return nullptr; });
  favicon_base::FaviconRawBitmapResult result;
  favicon_request_handler_.GetRawFaviconForPageURL(
      GURL(kDummyPageUrl), kDesiredSizeInPixel,
      base::BindOnce(&StoreBitmap, &result), FaviconRequestOrigin::UNKNOWN,
      kDummyPlatform,
      /*icon_url_for_uma=*/GURL(), synced_favicon_getter_.Get(),
      /*can_send_history_data=*/true, &tracker_);
  EXPECT_FALSE(result.is_valid());
  histogram_tester_.ExpectUniqueSample("Sync.FaviconAvailability.UNKNOWN",
                                       FaviconAvailability::kNotAvailable, 1);
}

TEST_F(FaviconRequestHandlerTest, ShouldGetSyncBitmap) {
  scoped_feature_list_.InitAndDisableFeature(
      kEnableHistoryFaviconsGoogleServerQuery);
  EXPECT_CALL(mock_favicon_service_,
              GetRawFaviconForPageURL(GURL(kDummyPageUrl), _,
                                      kDesiredSizeInPixel, _, _, &tracker_))
      .WillOnce([](auto, auto, auto, auto,
                   favicon_base::FaviconRawBitmapCallback callback, auto) {
        std::move(callback).Run(favicon_base::FaviconRawBitmapResult());
        return kDummyTaskId;
      });
  EXPECT_CALL(synced_favicon_getter_, Run(GURL(kDummyPageUrl)))
      .WillOnce([](auto) { return CreateTestBitmapBytes(); });
  favicon_base::FaviconRawBitmapResult result;
  favicon_request_handler_.GetRawFaviconForPageURL(
      GURL(kDummyPageUrl), kDesiredSizeInPixel,
      base::BindOnce(&StoreBitmap, &result), FaviconRequestOrigin::UNKNOWN,
      kDummyPlatform,
      /*icon_url_for_uma=*/GURL(), synced_favicon_getter_.Get(),
      /*can_send_history_data=*/true, &tracker_);
  EXPECT_TRUE(result.is_valid());
  histogram_tester_.ExpectUniqueSample("Sync.FaviconAvailability.UNKNOWN",
                                       FaviconAvailability::kSync, 1);
}

TEST_F(FaviconRequestHandlerTest, ShouldGetLocalBitmap) {
  scoped_feature_list_.InitAndDisableFeature(
      kEnableHistoryFaviconsGoogleServerQuery);
  EXPECT_CALL(mock_favicon_service_,
              GetRawFaviconForPageURL(GURL(kDummyPageUrl), _,
                                      kDesiredSizeInPixel, _, _, &tracker_))
      .WillOnce([](auto, auto, auto, auto,
                   favicon_base::FaviconRawBitmapCallback callback, auto) {
        std::move(callback).Run(CreateTestBitmapResult());
        return kDummyTaskId;
      });
  EXPECT_CALL(mock_large_icon_service_,
              TouchIconFromGoogleServer(GURL(kDummyIconUrl)));
  EXPECT_CALL(synced_favicon_getter_, Run(_)).Times(0);
  favicon_base::FaviconRawBitmapResult result;
  favicon_request_handler_.GetRawFaviconForPageURL(
      GURL(kDummyPageUrl), kDesiredSizeInPixel,
      base::BindOnce(&StoreBitmap, &result), FaviconRequestOrigin::UNKNOWN,
      kDummyPlatform,
      /*icon_url_for_uma=*/GURL(), synced_favicon_getter_.Get(),
      /*can_send_history_data=*/true, &tracker_);
  EXPECT_TRUE(result.is_valid());
  histogram_tester_.ExpectUniqueSample("Sync.FaviconAvailability.UNKNOWN",
                                       FaviconAvailability::kLocal, 1);
}

TEST_F(FaviconRequestHandlerTest, ShouldGetGoogleServerBitmapForFullUrl) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      kEnableHistoryFaviconsGoogleServerQuery, {{"trim_url_path", "false"}});
  EXPECT_CALL(mock_favicon_service_,
              GetRawFaviconForPageURL(GURL(kDummyPageUrl), _,
                                      kDesiredSizeInPixel, _, _, &tracker_))
      .WillOnce([](auto, auto, auto, auto,
                   favicon_base::FaviconRawBitmapCallback callback, auto) {
        std::move(callback).Run(favicon_base::FaviconRawBitmapResult());
        return kDummyTaskId;
      })
      .WillOnce([](auto, auto, auto, auto,
                   favicon_base::FaviconRawBitmapCallback callback, auto) {
        std::move(callback).Run(CreateTestBitmapResult());
        return kDummyTaskId;
      });
  EXPECT_CALL(mock_large_icon_service_,
              GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
                  _, _, /*should_trim_url_path=*/false, _, _))
      .WillOnce([](auto, auto, auto, auto,
                   favicon_base::GoogleFaviconServerCallback server_callback) {
        std::move(server_callback)
            .Run(favicon_base::GoogleFaviconServerRequestStatus::SUCCESS);
      });
  EXPECT_CALL(synced_favicon_getter_, Run(_)).Times(0);
  favicon_base::FaviconRawBitmapResult result;
  favicon_request_handler_.GetRawFaviconForPageURL(
      GURL(kDummyPageUrl), kDesiredSizeInPixel,
      base::BindOnce(&StoreBitmap, &result), FaviconRequestOrigin::HISTORY,
      kDummyPlatform,
      /*icon_url_for_uma=*/GURL(), synced_favicon_getter_.Get(),
      /*can_send_history_data=*/true, &tracker_);
  EXPECT_TRUE(result.is_valid());
}

TEST_F(FaviconRequestHandlerTest, ShouldGetGoogleServerBitmapForTrimmedUrl) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      kEnableHistoryFaviconsGoogleServerQuery, {{"trim_url_path", "true"}});
  EXPECT_CALL(mock_favicon_service_,
              GetRawFaviconForPageURL(GURL(kDummyPageUrl), _,
                                      kDesiredSizeInPixel, _, _, &tracker_))
      .WillOnce([](auto, auto, auto, auto,
                   favicon_base::FaviconRawBitmapCallback callback, auto) {
        std::move(callback).Run(favicon_base::FaviconRawBitmapResult());
        return kDummyTaskId;
      })
      .WillOnce([](auto, auto, auto, auto,
                   favicon_base::FaviconRawBitmapCallback callback, auto) {
        std::move(callback).Run(CreateTestBitmapResult());
        return kDummyTaskId;
      });
  EXPECT_CALL(mock_large_icon_service_,
              GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
                  _, _, /*should_trim_url_path=*/true, _, _))
      .WillOnce([](auto, auto, auto, auto,
                   favicon_base::GoogleFaviconServerCallback server_callback) {
        std::move(server_callback)
            .Run(favicon_base::GoogleFaviconServerRequestStatus::SUCCESS);
      });
  EXPECT_CALL(synced_favicon_getter_, Run(_)).Times(0);
  favicon_base::FaviconRawBitmapResult result;
  favicon_request_handler_.GetRawFaviconForPageURL(
      GURL(kDummyPageUrl), kDesiredSizeInPixel,
      base::BindOnce(&StoreBitmap, &result), FaviconRequestOrigin::HISTORY,
      kDummyPlatform,
      /*icon_url_for_uma=*/GURL(), synced_favicon_getter_.Get(),
      /*can_send_history_data=*/true, &tracker_);
  EXPECT_TRUE(result.is_valid());
  histogram_tester_.ExpectUniqueSample("Sync.FaviconAvailability.HISTORY",
                                       FaviconAvailability::kLocal, 1);
  histogram_tester_.ExpectUniqueSample(
      "Sync.SizeOfFaviconServerRequestGroup.HISTORY", 1, 1);
}

TEST_F(FaviconRequestHandlerTest, ShouldGetEmptyImage) {
  scoped_feature_list_.InitAndDisableFeature(
      kEnableHistoryFaviconsGoogleServerQuery);
  EXPECT_CALL(mock_favicon_service_,
              GetFaviconImageForPageURL(GURL(kDummyPageUrl), _, &tracker_))
      .WillOnce([](auto, favicon_base::FaviconImageCallback callback, auto) {
        std::move(callback).Run(favicon_base::FaviconImageResult());
        return kDummyTaskId;
      });
  EXPECT_CALL(synced_favicon_getter_, Run(GURL(kDummyPageUrl)))
      .WillOnce([](auto) { return nullptr; });
  favicon_base::FaviconImageResult result;
  favicon_request_handler_.GetFaviconImageForPageURL(
      GURL(kDummyPageUrl), base::BindOnce(&StoreImage, &result),
      FaviconRequestOrigin::UNKNOWN,
      /*icon_url_for_uma=*/GURL(), synced_favicon_getter_.Get(),
      /*can_send_history_data=*/true, &tracker_);
  EXPECT_TRUE(result.image.IsEmpty());
  histogram_tester_.ExpectUniqueSample("Sync.FaviconAvailability.UNKNOWN",
                                       FaviconAvailability::kNotAvailable, 1);
}

TEST_F(FaviconRequestHandlerTest, ShouldGetSyncImage) {
  scoped_feature_list_.InitAndDisableFeature(
      kEnableHistoryFaviconsGoogleServerQuery);
  EXPECT_CALL(mock_favicon_service_,
              GetFaviconImageForPageURL(GURL(kDummyPageUrl), _, &tracker_))
      .WillOnce([](auto, favicon_base::FaviconImageCallback callback, auto) {
        std::move(callback).Run(favicon_base::FaviconImageResult());
        return kDummyTaskId;
      });
  EXPECT_CALL(synced_favicon_getter_, Run(GURL(kDummyPageUrl)))
      .WillOnce([](auto) { return CreateTestBitmapBytes(); });
  favicon_base::FaviconImageResult result;
  favicon_request_handler_.GetFaviconImageForPageURL(
      GURL(kDummyPageUrl), base::BindOnce(&StoreImage, &result),
      FaviconRequestOrigin::UNKNOWN,
      /*icon_url_for_uma=*/GURL(), synced_favicon_getter_.Get(),
      /*can_send_history_data=*/true, &tracker_);
  EXPECT_FALSE(result.image.IsEmpty());
  histogram_tester_.ExpectUniqueSample("Sync.FaviconAvailability.UNKNOWN",
                                       FaviconAvailability::kSync, 1);
}

TEST_F(FaviconRequestHandlerTest, ShouldGetLocalImage) {
  scoped_feature_list_.InitAndDisableFeature(
      kEnableHistoryFaviconsGoogleServerQuery);
  EXPECT_CALL(mock_favicon_service_,
              GetFaviconImageForPageURL(GURL(kDummyPageUrl), _, &tracker_))
      .WillOnce([](auto, favicon_base::FaviconImageCallback callback, auto) {
        std::move(callback).Run(CreateTestImageResult());
        return kDummyTaskId;
      });
  EXPECT_CALL(mock_large_icon_service_,
              TouchIconFromGoogleServer(GURL(kDummyIconUrl)));
  EXPECT_CALL(synced_favicon_getter_, Run(_)).Times(0);
  favicon_base::FaviconImageResult result;
  favicon_request_handler_.GetFaviconImageForPageURL(
      GURL(kDummyPageUrl), base::BindOnce(&StoreImage, &result),
      FaviconRequestOrigin::UNKNOWN,
      /*icon_url_for_uma=*/GURL(), synced_favicon_getter_.Get(),
      /*can_send_history_data=*/true, &tracker_);
  EXPECT_FALSE(result.image.IsEmpty());
  histogram_tester_.ExpectUniqueSample("Sync.FaviconAvailability.UNKNOWN",
                                       FaviconAvailability::kLocal, 1);
}

TEST_F(FaviconRequestHandlerTest, ShouldGetGoogleServerImageForFullUrl) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      kEnableHistoryFaviconsGoogleServerQuery, {{"trim_url_path", "false"}});
  EXPECT_CALL(mock_favicon_service_,
              GetFaviconImageForPageURL(GURL(kDummyPageUrl), _, &tracker_))
      .WillOnce([](auto, favicon_base::FaviconImageCallback callback, auto) {
        std::move(callback).Run(favicon_base::FaviconImageResult());
        return kDummyTaskId;
      })
      .WillOnce([](auto, favicon_base::FaviconImageCallback callback, auto) {
        std::move(callback).Run(CreateTestImageResult());
        return kDummyTaskId;
      });
  EXPECT_CALL(mock_large_icon_service_,
              GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
                  _, _, /*should_trim_url_path=*/false, _, _))
      .WillOnce([](auto, auto, auto, auto,
                   favicon_base::GoogleFaviconServerCallback server_callback) {
        std::move(server_callback)
            .Run(favicon_base::GoogleFaviconServerRequestStatus::SUCCESS);
      });
  EXPECT_CALL(synced_favicon_getter_, Run(_)).Times(0);
  favicon_base::FaviconImageResult result;
  favicon_request_handler_.GetFaviconImageForPageURL(
      GURL(kDummyPageUrl), base::BindOnce(&StoreImage, &result),
      FaviconRequestOrigin::RECENTLY_CLOSED_TABS, /*icon_url_for_uma=*/GURL(),
      synced_favicon_getter_.Get(), /*can_send_history_data=*/true, &tracker_);
  EXPECT_FALSE(result.image.IsEmpty());
  histogram_tester_.ExpectUniqueSample(
      "Sync.FaviconAvailability.RECENTLY_CLOSED_TABS",
      FaviconAvailability::kLocal, 1);
  histogram_tester_.ExpectUniqueSample(
      "Sync.SizeOfFaviconServerRequestGroup.RECENTLY_CLOSED_TABS", 1, 1);
}

TEST_F(FaviconRequestHandlerTest, ShouldGetGoogleServerImageForTrimmedUrl) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      kEnableHistoryFaviconsGoogleServerQuery, {{"trim_url_path", "true"}});
  EXPECT_CALL(mock_favicon_service_,
              GetFaviconImageForPageURL(GURL(kDummyPageUrl), _, &tracker_))
      .WillOnce([](auto, favicon_base::FaviconImageCallback callback, auto) {
        std::move(callback).Run(favicon_base::FaviconImageResult());
        return kDummyTaskId;
      })
      .WillOnce([](auto, favicon_base::FaviconImageCallback callback, auto) {
        std::move(callback).Run(CreateTestImageResult());
        return kDummyTaskId;
      });
  EXPECT_CALL(mock_large_icon_service_,
              GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
                  _, _, /*should_trim_url_path=*/true, _, _))
      .WillOnce([](auto, auto, auto, auto,
                   favicon_base::GoogleFaviconServerCallback server_callback) {
        std::move(server_callback)
            .Run(favicon_base::GoogleFaviconServerRequestStatus::SUCCESS);
      });
  EXPECT_CALL(synced_favicon_getter_, Run(_)).Times(0);
  favicon_base::FaviconImageResult result;
  favicon_request_handler_.GetFaviconImageForPageURL(
      GURL(kDummyPageUrl), base::BindOnce(&StoreImage, &result),
      FaviconRequestOrigin::RECENTLY_CLOSED_TABS, /*icon_url_for_uma=*/GURL(),
      synced_favicon_getter_.Get(),
      /*can_send_history_data=*/true, &tracker_);
  EXPECT_FALSE(result.image.IsEmpty());
}

TEST_F(FaviconRequestHandlerTest, ShouldNotQueryGoogleServerIfCannotSendData) {
  scoped_feature_list_.InitAndEnableFeature(
      kEnableHistoryFaviconsGoogleServerQuery);
  EXPECT_CALL(mock_favicon_service_,
              GetRawFaviconForPageURL(GURL(kDummyPageUrl), _,
                                      kDesiredSizeInPixel, _, _, &tracker_))
      .WillOnce([](auto, auto, auto, auto,
                   favicon_base::FaviconRawBitmapCallback callback, auto) {
        std::move(callback).Run(favicon_base::FaviconRawBitmapResult());
        return kDummyTaskId;
      });
  EXPECT_CALL(mock_large_icon_service_,
              GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
                  _, _, _, _, _))
      .Times(0);
  favicon_base::FaviconRawBitmapResult result;
  favicon_request_handler_.GetRawFaviconForPageURL(
      GURL(kDummyPageUrl), kDesiredSizeInPixel,
      base::BindOnce(&StoreBitmap, &result), FaviconRequestOrigin::HISTORY,
      kDummyPlatform,
      /*icon_url_for_uma=*/GURL(), synced_favicon_getter_.Get(),
      /*can_send_history_data=*/false, &tracker_);
}

}  // namespace
}  // namespace favicon
