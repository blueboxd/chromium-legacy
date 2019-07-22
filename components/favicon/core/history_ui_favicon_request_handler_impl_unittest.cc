// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/history_ui_favicon_request_handler_impl.h"

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
#include "components/favicon_base/favicon_types.h"
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
using testing::Return;

// TODO(victorvianna): Rename to kPageUrl and kIconUrl.
const char kDummyPageUrl[] = "https://www.example.com";
const char kDummyIconUrl[] = "https://www.example.com/favicon16.png";
const HistoryUiFaviconRequestOrigin kDummyOrigin =
    HistoryUiFaviconRequestOrigin::kHistory;
const char kDummyOriginHistogramSuffix[] = ".HISTORY";
const FaviconRequestPlatform kDummyPlatform = FaviconRequestPlatform::kDesktop;
base::CancelableTaskTracker::TaskId kDummyTaskId = 1;
const char kAvailabilityHistogramName[] =
    "Sync.SyncedHistoryFaviconAvailability";
const char kGroupingHistogramName[] =
    "Sync.RequestGroupSizeForSyncedHistoryFavicons";
const int kDefaultDesiredSizeInPixel = 16;
// TODO(victorvianna): Add unit tests specific for mobile.
const SkColor kTestColor = SK_ColorRED;

// Custom matcher for FaviconServerFetcherParams.
MATCHER_P(FetcherParamsPageUrlEq, url, "") {
  return arg->page_url() == GURL(url);
}

SkBitmap CreateTestSkBitmap(int desired_size_in_pixel) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(desired_size_in_pixel, desired_size_in_pixel);
  bitmap.eraseColor(kTestColor);
  return bitmap;
}

favicon_base::FaviconRawBitmapResult CreateTestBitmapResult(
    int desired_size_in_pixel) {
  scoped_refptr<base::RefCountedBytes> data(new base::RefCountedBytes());
  gfx::PNGCodec::EncodeBGRASkBitmap(CreateTestSkBitmap(desired_size_in_pixel),
                                    false, &data->data());
  favicon_base::FaviconRawBitmapResult result;
  result.bitmap_data = data;
  result.icon_url = GURL(kDummyIconUrl);
  result.pixel_size = gfx::Size(desired_size_in_pixel, desired_size_in_pixel);
  return result;
}

favicon_base::FaviconRawBitmapResult CreateTestBitmapResult() {
  return CreateTestBitmapResult(kDefaultDesiredSizeInPixel);
}

favicon_base::FaviconImageResult CreateTestImageResult() {
  favicon_base::FaviconImageResult result;
  result.image = gfx::Image::CreateFrom1xBitmap(
      CreateTestSkBitmap(kDefaultDesiredSizeInPixel));
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

class HistoryUiFaviconRequestHandlerImplTest : public ::testing::Test {
 public:
  HistoryUiFaviconRequestHandlerImplTest()
      : history_ui_favicon_request_handler_(synced_favicon_getter_.Get(),
                                            can_send_history_data_getter_.Get(),
                                            &mock_favicon_service_,
                                            &mock_large_icon_service_) {
    ON_CALL(can_send_history_data_getter_, Run()).WillByDefault(Return(true));
  }

 protected:
  testing::NiceMock<MockFaviconService> mock_favicon_service_;
  testing::NiceMock<MockLargeIconService> mock_large_icon_service_;
  testing::NiceMock<base::MockCallback<
      HistoryUiFaviconRequestHandlerImpl::SyncedFaviconGetter>>
      synced_favicon_getter_;
  testing::NiceMock<base::MockCallback<
      HistoryUiFaviconRequestHandlerImpl::CanSendHistoryDataGetter>>
      can_send_history_data_getter_;
  base::CancelableTaskTracker tracker_;
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList scoped_feature_list_;
  HistoryUiFaviconRequestHandlerImpl history_ui_favicon_request_handler_;
};

TEST_F(HistoryUiFaviconRequestHandlerImplTest, ShouldGetEmptyBitmap) {
  scoped_feature_list_.InitAndDisableFeature(
      kEnableHistoryFaviconsGoogleServerQuery);
  EXPECT_CALL(
      mock_favicon_service_,
      GetRawFaviconForPageURL(GURL(kDummyPageUrl), _,
                              kDefaultDesiredSizeInPixel, _, _, &tracker_))
      .WillOnce([](auto, auto, auto, auto,
                   favicon_base::FaviconRawBitmapCallback callback, auto) {
        std::move(callback).Run(favicon_base::FaviconRawBitmapResult());
        return kDummyTaskId;
      });
  EXPECT_CALL(synced_favicon_getter_, Run(GURL(kDummyPageUrl)))
      .WillOnce([](auto) { return favicon_base::FaviconRawBitmapResult(); });
  favicon_base::FaviconRawBitmapResult result;
  history_ui_favicon_request_handler_.GetRawFaviconForPageURL(
      GURL(kDummyPageUrl), kDefaultDesiredSizeInPixel,
      base::BindOnce(&StoreBitmap, &result), kDummyPlatform, kDummyOrigin,
      /*icon_url_for_uma=*/GURL(), &tracker_);
  EXPECT_FALSE(result.is_valid());
  histogram_tester_.ExpectUniqueSample(
      std::string(kAvailabilityHistogramName) + kDummyOriginHistogramSuffix,
      FaviconAvailability::kNotAvailable, 1);
}

TEST_F(HistoryUiFaviconRequestHandlerImplTest, ShouldGetSyncBitmap) {
  scoped_feature_list_.InitAndDisableFeature(
      kEnableHistoryFaviconsGoogleServerQuery);
  EXPECT_CALL(
      mock_favicon_service_,
      GetRawFaviconForPageURL(GURL(kDummyPageUrl), _,
                              kDefaultDesiredSizeInPixel, _, _, &tracker_))
      .WillOnce([](auto, auto, auto, auto,
                   favicon_base::FaviconRawBitmapCallback callback, auto) {
        std::move(callback).Run(favicon_base::FaviconRawBitmapResult());
        return kDummyTaskId;
      });
  EXPECT_CALL(synced_favicon_getter_, Run(GURL(kDummyPageUrl)))
      .WillOnce([](auto) { return CreateTestBitmapResult(); });
  favicon_base::FaviconRawBitmapResult result;
  history_ui_favicon_request_handler_.GetRawFaviconForPageURL(
      GURL(kDummyPageUrl), kDefaultDesiredSizeInPixel,
      base::BindOnce(&StoreBitmap, &result), kDummyPlatform, kDummyOrigin,
      /*icon_url_for_uma=*/GURL(), &tracker_);
  EXPECT_TRUE(result.is_valid());
  histogram_tester_.ExpectUniqueSample(
      std::string(kAvailabilityHistogramName) + kDummyOriginHistogramSuffix,
      FaviconAvailability::kSync, 1);
}

TEST_F(HistoryUiFaviconRequestHandlerImplTest, ShouldGetLocalBitmap) {
  scoped_feature_list_.InitAndDisableFeature(
      kEnableHistoryFaviconsGoogleServerQuery);
  EXPECT_CALL(
      mock_favicon_service_,
      GetRawFaviconForPageURL(GURL(kDummyPageUrl), _,
                              kDefaultDesiredSizeInPixel, _, _, &tracker_))
      .WillOnce([](auto, auto, auto, auto,
                   favicon_base::FaviconRawBitmapCallback callback, auto) {
        std::move(callback).Run(CreateTestBitmapResult());
        return kDummyTaskId;
      });
  EXPECT_CALL(mock_large_icon_service_,
              TouchIconFromGoogleServer(GURL(kDummyIconUrl)));
  EXPECT_CALL(synced_favicon_getter_, Run(_)).Times(0);
  favicon_base::FaviconRawBitmapResult result;
  history_ui_favicon_request_handler_.GetRawFaviconForPageURL(
      GURL(kDummyPageUrl), kDefaultDesiredSizeInPixel,
      base::BindOnce(&StoreBitmap, &result), kDummyPlatform, kDummyOrigin,
      /*icon_url_for_uma=*/GURL(), &tracker_);
  EXPECT_TRUE(result.is_valid());
  histogram_tester_.ExpectUniqueSample(
      std::string(kAvailabilityHistogramName) + kDummyOriginHistogramSuffix,
      FaviconAvailability::kLocal, 1);
}

TEST_F(HistoryUiFaviconRequestHandlerImplTest,
       ShouldGetGoogleServerBitmapForFullUrl) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      kEnableHistoryFaviconsGoogleServerQuery, {{"trim_url_path", "false"}});
  EXPECT_CALL(can_send_history_data_getter_, Run());
  EXPECT_CALL(
      mock_favicon_service_,
      GetRawFaviconForPageURL(GURL(kDummyPageUrl), _,
                              kDefaultDesiredSizeInPixel, _, _, &tracker_))
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
                  FetcherParamsPageUrlEq(kDummyPageUrl), _,
                  /*should_trim_url_path=*/false, _, _))
      .WillOnce([](auto, auto, auto, auto,
                   favicon_base::GoogleFaviconServerCallback server_callback) {
        std::move(server_callback)
            .Run(favicon_base::GoogleFaviconServerRequestStatus::SUCCESS);
      });
  EXPECT_CALL(synced_favicon_getter_, Run(_)).Times(0);
  favicon_base::FaviconRawBitmapResult result;
  history_ui_favicon_request_handler_.GetRawFaviconForPageURL(
      GURL(kDummyPageUrl), kDefaultDesiredSizeInPixel,
      base::BindOnce(&StoreBitmap, &result), kDummyPlatform, kDummyOrigin,
      /*icon_url_for_uma=*/GURL(), &tracker_);
  EXPECT_TRUE(result.is_valid());
  histogram_tester_.ExpectUniqueSample(
      std::string(kAvailabilityHistogramName) + kDummyOriginHistogramSuffix,
      FaviconAvailability::kLocal, 1);
  histogram_tester_.ExpectUniqueSample(
      std::string(kGroupingHistogramName) + kDummyOriginHistogramSuffix, 1, 1);
}

TEST_F(HistoryUiFaviconRequestHandlerImplTest,
       ShouldGetGoogleServerBitmapForTrimmedUrl) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      kEnableHistoryFaviconsGoogleServerQuery, {{"trim_url_path", "true"}});
  EXPECT_CALL(can_send_history_data_getter_, Run());
  EXPECT_CALL(
      mock_favicon_service_,
      GetRawFaviconForPageURL(GURL(kDummyPageUrl), _,
                              kDefaultDesiredSizeInPixel, _, _, &tracker_))
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
                  FetcherParamsPageUrlEq(kDummyPageUrl), _,
                  /*should_trim_url_path=*/true, _, _))
      .WillOnce([](auto, auto, auto, auto,
                   favicon_base::GoogleFaviconServerCallback server_callback) {
        std::move(server_callback)
            .Run(favicon_base::GoogleFaviconServerRequestStatus::SUCCESS);
      });
  EXPECT_CALL(synced_favicon_getter_, Run(_)).Times(0);
  favicon_base::FaviconRawBitmapResult result;
  history_ui_favicon_request_handler_.GetRawFaviconForPageURL(
      GURL(kDummyPageUrl), kDefaultDesiredSizeInPixel,
      base::BindOnce(&StoreBitmap, &result), kDummyPlatform, kDummyOrigin,
      /*icon_url_for_uma=*/GURL(), &tracker_);
  EXPECT_TRUE(result.is_valid());
  histogram_tester_.ExpectUniqueSample(
      std::string(kAvailabilityHistogramName) + kDummyOriginHistogramSuffix,
      FaviconAvailability::kLocal, 1);
  histogram_tester_.ExpectUniqueSample(
      std::string(kGroupingHistogramName) + kDummyOriginHistogramSuffix, 1, 1);
}

TEST_F(HistoryUiFaviconRequestHandlerImplTest, ShouldGetEmptyImage) {
  scoped_feature_list_.InitAndDisableFeature(
      kEnableHistoryFaviconsGoogleServerQuery);
  EXPECT_CALL(mock_favicon_service_,
              GetFaviconImageForPageURL(GURL(kDummyPageUrl), _, &tracker_))
      .WillOnce([](auto, favicon_base::FaviconImageCallback callback, auto) {
        std::move(callback).Run(favicon_base::FaviconImageResult());
        return kDummyTaskId;
      });
  EXPECT_CALL(synced_favicon_getter_, Run(GURL(kDummyPageUrl)))
      .WillOnce([](auto) { return favicon_base::FaviconRawBitmapResult(); });
  favicon_base::FaviconImageResult result;
  history_ui_favicon_request_handler_.GetFaviconImageForPageURL(
      GURL(kDummyPageUrl), base::BindOnce(&StoreImage, &result), kDummyOrigin,
      /*icon_url_for_uma=*/GURL(), &tracker_);
  EXPECT_TRUE(result.image.IsEmpty());
  histogram_tester_.ExpectUniqueSample(
      std::string(kAvailabilityHistogramName) + kDummyOriginHistogramSuffix,
      FaviconAvailability::kNotAvailable, 1);
}

TEST_F(HistoryUiFaviconRequestHandlerImplTest, ShouldGetSyncImage) {
  scoped_feature_list_.InitAndDisableFeature(
      kEnableHistoryFaviconsGoogleServerQuery);
  EXPECT_CALL(mock_favicon_service_,
              GetFaviconImageForPageURL(GURL(kDummyPageUrl), _, &tracker_))
      .WillOnce([](auto, favicon_base::FaviconImageCallback callback, auto) {
        std::move(callback).Run(favicon_base::FaviconImageResult());
        return kDummyTaskId;
      });
  EXPECT_CALL(synced_favicon_getter_, Run(GURL(kDummyPageUrl)))
      .WillOnce([](auto) { return CreateTestBitmapResult(); });
  favicon_base::FaviconImageResult result;
  history_ui_favicon_request_handler_.GetFaviconImageForPageURL(
      GURL(kDummyPageUrl), base::BindOnce(&StoreImage, &result), kDummyOrigin,
      /*icon_url_for_uma=*/GURL(), &tracker_);
  EXPECT_FALSE(result.image.IsEmpty());
  histogram_tester_.ExpectUniqueSample(
      std::string(kAvailabilityHistogramName) + kDummyOriginHistogramSuffix,
      FaviconAvailability::kSync, 1);
}

TEST_F(HistoryUiFaviconRequestHandlerImplTest, ShouldGetLocalImage) {
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
  history_ui_favicon_request_handler_.GetFaviconImageForPageURL(
      GURL(kDummyPageUrl), base::BindOnce(&StoreImage, &result), kDummyOrigin,
      /*icon_url_for_uma=*/GURL(), &tracker_);
  EXPECT_FALSE(result.image.IsEmpty());
  histogram_tester_.ExpectUniqueSample(
      std::string(kAvailabilityHistogramName) + kDummyOriginHistogramSuffix,
      FaviconAvailability::kLocal, 1);
}

TEST_F(HistoryUiFaviconRequestHandlerImplTest,
       ShouldGetGoogleServerImageForFullUrl) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      kEnableHistoryFaviconsGoogleServerQuery, {{"trim_url_path", "false"}});
  EXPECT_CALL(can_send_history_data_getter_, Run());
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
                  FetcherParamsPageUrlEq(kDummyPageUrl), _,
                  /*should_trim_url_path=*/false, _, _))
      .WillOnce([](auto, auto, auto, auto,
                   favicon_base::GoogleFaviconServerCallback server_callback) {
        std::move(server_callback)
            .Run(favicon_base::GoogleFaviconServerRequestStatus::SUCCESS);
      });
  EXPECT_CALL(synced_favicon_getter_, Run(_)).Times(0);
  favicon_base::FaviconImageResult result;
  history_ui_favicon_request_handler_.GetFaviconImageForPageURL(
      GURL(kDummyPageUrl), base::BindOnce(&StoreImage, &result), kDummyOrigin,
      /*icon_url_for_uma=*/GURL(), &tracker_);
  EXPECT_FALSE(result.image.IsEmpty());
  histogram_tester_.ExpectUniqueSample(
      std::string(kAvailabilityHistogramName) + kDummyOriginHistogramSuffix,
      FaviconAvailability::kLocal, 1);
  histogram_tester_.ExpectUniqueSample(
      std::string(kGroupingHistogramName) + kDummyOriginHistogramSuffix, 1, 1);
}

TEST_F(HistoryUiFaviconRequestHandlerImplTest,
       ShouldGetGoogleServerImageForTrimmedUrl) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      kEnableHistoryFaviconsGoogleServerQuery, {{"trim_url_path", "true"}});
  EXPECT_CALL(can_send_history_data_getter_, Run());
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
                  FetcherParamsPageUrlEq(kDummyPageUrl), _,
                  /*should_trim_url_path=*/true, _, _))
      .WillOnce([](auto, auto, auto, auto,
                   favicon_base::GoogleFaviconServerCallback server_callback) {
        std::move(server_callback)
            .Run(favicon_base::GoogleFaviconServerRequestStatus::SUCCESS);
      });
  EXPECT_CALL(synced_favicon_getter_, Run(_)).Times(0);
  favicon_base::FaviconImageResult result;
  history_ui_favicon_request_handler_.GetFaviconImageForPageURL(
      GURL(kDummyPageUrl), base::BindOnce(&StoreImage, &result), kDummyOrigin,
      /*icon_url_for_uma=*/GURL(), &tracker_);
  EXPECT_FALSE(result.image.IsEmpty());
  histogram_tester_.ExpectUniqueSample(
      std::string(kAvailabilityHistogramName) + kDummyOriginHistogramSuffix,
      FaviconAvailability::kLocal, 1);
  histogram_tester_.ExpectUniqueSample(
      std::string(kGroupingHistogramName) + kDummyOriginHistogramSuffix, 1, 1);
}

TEST_F(HistoryUiFaviconRequestHandlerImplTest,
       ShouldNotQueryGoogleServerIfCannotSendData) {
  scoped_feature_list_.InitAndEnableFeature(
      kEnableHistoryFaviconsGoogleServerQuery);
  EXPECT_CALL(can_send_history_data_getter_, Run()).WillOnce([]() {
    return false;
  });
  EXPECT_CALL(
      mock_favicon_service_,
      GetRawFaviconForPageURL(GURL(kDummyPageUrl), _,
                              kDefaultDesiredSizeInPixel, _, _, &tracker_))
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
  history_ui_favicon_request_handler_.GetRawFaviconForPageURL(
      GURL(kDummyPageUrl), kDefaultDesiredSizeInPixel,
      base::BindOnce(&StoreBitmap, &result), kDummyPlatform, kDummyOrigin,
      /*icon_url_for_uma=*/GURL(), &tracker_);
}

TEST_F(HistoryUiFaviconRequestHandlerImplTest, ShouldResizeSyncBitmap) {
  const int kDesiredSizeInPixel = 32;
  base::test::ScopedFeatureList scoped_feature_list;
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
  // Have sync return bitmap of different size from the one requested.
  EXPECT_CALL(synced_favicon_getter_, Run(GURL(kDummyPageUrl)))
      .WillOnce([](auto) { return CreateTestBitmapResult(16); });
  favicon_base::FaviconRawBitmapResult result;
  history_ui_favicon_request_handler_.GetRawFaviconForPageURL(
      GURL(kDummyPageUrl), kDesiredSizeInPixel,
      base::BindOnce(&StoreBitmap, &result), kDummyPlatform, kDummyOrigin,
      /*icon_url_for_uma=*/GURL(), &tracker_);
  EXPECT_TRUE(result.is_valid());
  EXPECT_EQ(gfx::Size(kDesiredSizeInPixel, kDesiredSizeInPixel),
            result.pixel_size);
}

}  // namespace
}  // namespace favicon
