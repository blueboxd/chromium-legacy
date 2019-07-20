// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/local_frame.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

namespace {

void MaybeAllowImagePlaceholder(DummyPageHolder* page_holder,
                                FetchParameters& params) {
  if (page_holder->GetFrame().IsClientLoFiAllowed(params.GetResourceRequest()))
    params.SetClientLoFiPlaceholder();
}

void DisableLazyLoadAndDisableDataSaverHoldbackInSettings(Settings& settings) {
  settings.SetLazyLoadEnabled(false);
  settings.SetDataSaverHoldbackWebApi(false);
}
void EnableLazyLoadAndDisableDataSaverHoldbackInSettings(Settings& settings) {
  settings.SetLazyLoadEnabled(true);
  settings.SetDataSaverHoldbackWebApi(false);
}
void EnableLazyLoadAndEnableDataSaverHoldbackInSettings(Settings& settings) {
  settings.SetLazyLoadEnabled(true);
  settings.SetDataSaverHoldbackWebApi(true);
}

}  // namespace

class TestLocalFrameClient : public EmptyLocalFrameClient {
 public:
  explicit TestLocalFrameClient(
      WebURLRequest::PreviewsState frame_previews_state)
      : frame_previews_state_(frame_previews_state) {}
  ~TestLocalFrameClient() override = default;

  WebURLRequest::PreviewsState GetPreviewsStateForFrame() const override {
    return frame_previews_state_;
  }

 private:
  const WebURLRequest::PreviewsState frame_previews_state_;
};

class LocalFrameTest : public testing::Test {
 public:
  void TearDown() override {
    // Reset the global data saver setting to false at the end of the test.
    GetNetworkStateNotifier().SetSaveDataEnabled(false);
  }
};

TEST_F(LocalFrameTest, MaybeAllowPlaceholderImageUsesSpecifiedRequestValue) {
  ResourceRequest request1;
  request1.SetUrl(KURL("http://insecure.com"));
  request1.SetPreviewsState(WebURLRequest::kClientLoFiOn);
  FetchParameters params1(request1);
  auto page_holder = std::make_unique<DummyPageHolder>(
      IntSize(800, 600), nullptr,
      MakeGarbageCollected<TestLocalFrameClient>(WebURLRequest::kPreviewsOff));
  MaybeAllowImagePlaceholder(page_holder.get(), params1);
  EXPECT_EQ(FetchParameters::kAllowPlaceholder,
            params1.GetImageRequestOptimization());

  ResourceRequest request2;
  request2.SetUrl(KURL("https://secure.com"));
  request2.SetPreviewsState(WebURLRequest::kPreviewsOff);
  FetchParameters params2(request2);
  auto page_holder2 = std::make_unique<DummyPageHolder>(
      IntSize(800, 600), nullptr,
      MakeGarbageCollected<TestLocalFrameClient>(WebURLRequest::kClientLoFiOn));
  MaybeAllowImagePlaceholder(page_holder2.get(), params2);
  EXPECT_EQ(FetchParameters::kNone, params2.GetImageRequestOptimization());
}

TEST_F(LocalFrameTest, MaybeAllowPlaceholderImageUsesFramePreviewsState) {
  ResourceRequest request1;
  request1.SetUrl(KURL("http://insecure.com"));
  request1.SetPreviewsState(WebURLRequest::kPreviewsUnspecified);
  FetchParameters params1(request1);
  auto page_holder = std::make_unique<DummyPageHolder>(
      IntSize(800, 600), nullptr,
      MakeGarbageCollected<TestLocalFrameClient>(WebURLRequest::kClientLoFiOn));
  MaybeAllowImagePlaceholder(page_holder.get(), params1);
  EXPECT_EQ(FetchParameters::kAllowPlaceholder,
            params1.GetImageRequestOptimization());
  EXPECT_TRUE(page_holder->GetFrame().IsUsingDataSavingPreview());

  ResourceRequest request2;
  request2.SetUrl(KURL("http://insecure.com"));
  request2.SetPreviewsState(WebURLRequest::kPreviewsUnspecified);
  FetchParameters params2(request2);
  auto page_holder2 = std::make_unique<DummyPageHolder>(
      IntSize(800, 600), nullptr,
      MakeGarbageCollected<TestLocalFrameClient>(
          WebURLRequest::kServerLitePageOn));
  MaybeAllowImagePlaceholder(page_holder2.get(), params2);
  EXPECT_EQ(FetchParameters::kNone, params2.GetImageRequestOptimization());
  EXPECT_FALSE(page_holder2->GetFrame().IsUsingDataSavingPreview());
}

TEST_F(LocalFrameTest, IsUsingDataSavingPreview) {
  EXPECT_TRUE(std::make_unique<DummyPageHolder>(
                  IntSize(800, 600), nullptr,
                  MakeGarbageCollected<TestLocalFrameClient>(
                      WebURLRequest::kClientLoFiOn))
                  ->GetFrame()
                  .IsUsingDataSavingPreview());
  EXPECT_TRUE(std::make_unique<DummyPageHolder>(
                  IntSize(800, 600), nullptr,
                  MakeGarbageCollected<TestLocalFrameClient>(
                      WebURLRequest::kNoScriptOn))
                  ->GetFrame()
                  .IsUsingDataSavingPreview());

  EXPECT_FALSE(std::make_unique<DummyPageHolder>(
                   IntSize(800, 600), nullptr,
                   MakeGarbageCollected<TestLocalFrameClient>(
                       WebURLRequest::kPreviewsUnspecified))
                   ->GetFrame()
                   .IsUsingDataSavingPreview());
  EXPECT_FALSE(std::make_unique<DummyPageHolder>(
                   IntSize(800, 600), nullptr,
                   MakeGarbageCollected<TestLocalFrameClient>(
                       WebURLRequest::kPreviewsOff))
                   ->GetFrame()
                   .IsUsingDataSavingPreview());
  EXPECT_FALSE(std::make_unique<DummyPageHolder>(
                   IntSize(800, 600), nullptr,
                   MakeGarbageCollected<TestLocalFrameClient>(
                       WebURLRequest::kPreviewsNoTransform))
                   ->GetFrame()
                   .IsUsingDataSavingPreview());
  EXPECT_FALSE(std::make_unique<DummyPageHolder>(
                   IntSize(800, 600), nullptr,
                   MakeGarbageCollected<TestLocalFrameClient>(
                       WebURLRequest::kServerLitePageOn))
                   ->GetFrame()
                   .IsUsingDataSavingPreview());
}

TEST_F(LocalFrameTest, IsLazyLoadingImageAllowedWithFeatureDisabled) {
  ScopedLazyImageLoadingForTest scoped_lazy_image_loading_for_test(false);
  auto page_holder = std::make_unique<DummyPageHolder>(
      IntSize(800, 600), nullptr, nullptr,
      base::BindOnce(&EnableLazyLoadAndDisableDataSaverHoldbackInSettings));
  EXPECT_EQ(LocalFrame::LazyLoadImageSetting::kDisabled,
            page_holder->GetFrame().GetLazyLoadImageSetting());
}

TEST_F(LocalFrameTest, IsLazyLoadingImageAllowedWithSettingDisabled) {
  ScopedLazyImageLoadingForTest scoped_lazy_image_loading_for_test(false);
  auto page_holder = std::make_unique<DummyPageHolder>(
      IntSize(800, 600), nullptr, nullptr,
      base::BindOnce(&DisableLazyLoadAndDisableDataSaverHoldbackInSettings));
  EXPECT_EQ(LocalFrame::LazyLoadImageSetting::kDisabled,
            page_holder->GetFrame().GetLazyLoadImageSetting());
}

TEST_F(LocalFrameTest, IsLazyLoadingImageAllowedWithAutomaticDisabled) {
  ScopedLazyImageLoadingForTest scoped_lazy_image_loading_for_test(true);
  ScopedAutomaticLazyImageLoadingForTest
      scoped_automatic_lazy_image_loading_for_test(false);
  auto page_holder = std::make_unique<DummyPageHolder>(
      IntSize(800, 600), nullptr, nullptr,
      base::BindOnce(&EnableLazyLoadAndDisableDataSaverHoldbackInSettings));
  EXPECT_EQ(LocalFrame::LazyLoadImageSetting::kEnabledExplicit,
            page_holder->GetFrame().GetLazyLoadImageSetting());
}

TEST_F(LocalFrameTest, IsLazyLoadingImageAllowedWhenNotRestricted) {
  ScopedLazyImageLoadingForTest scoped_lazy_image_loading_for_test(true);
  ScopedAutomaticLazyImageLoadingForTest
      scoped_automatic_lazy_image_loading_for_test(true);
  ScopedRestrictAutomaticLazyImageLoadingToDataSaverForTest
      scoped_restrict_automatic_lazy_image_loading_to_data_saver_for_test(
          false);
  auto page_holder = std::make_unique<DummyPageHolder>(
      IntSize(800, 600), nullptr, nullptr,
      base::BindOnce(&EnableLazyLoadAndDisableDataSaverHoldbackInSettings));
  EXPECT_EQ(LocalFrame::LazyLoadImageSetting::kEnabledAutomatic,
            page_holder->GetFrame().GetLazyLoadImageSetting());
}

TEST_F(LocalFrameTest,
       IsLazyLoadingImageAllowedWhenRestrictedWithDataSaverDisabled) {
  ScopedLazyImageLoadingForTest scoped_lazy_image_loading_for_test(true);
  ScopedAutomaticLazyImageLoadingForTest
      scoped_automatic_lazy_image_loading_for_test(true);
  ScopedRestrictAutomaticLazyImageLoadingToDataSaverForTest
      scoped_restrict_automatic_lazy_image_loading_to_data_saver_for_test(true);
  GetNetworkStateNotifier().SetSaveDataEnabled(false);
  auto page_holder = std::make_unique<DummyPageHolder>(
      IntSize(800, 600), nullptr, nullptr,
      base::BindOnce(&EnableLazyLoadAndDisableDataSaverHoldbackInSettings));
  EXPECT_EQ(LocalFrame::LazyLoadImageSetting::kEnabledExplicit,
            page_holder->GetFrame().GetLazyLoadImageSetting());
}

TEST_F(LocalFrameTest,
       IsLazyLoadingImageAllowedWhenRestrictedWithDataSaverEnabledHoldback) {
  ScopedLazyImageLoadingForTest scoped_lazy_image_loading_for_test(true);
  ScopedAutomaticLazyImageLoadingForTest
      scoped_automatic_lazy_image_loading_for_test(true);
  ScopedRestrictAutomaticLazyImageLoadingToDataSaverForTest
      scoped_restrict_automatic_lazy_image_loading_to_data_saver_for_test(true);
  GetNetworkStateNotifier().SetSaveDataEnabled(true);
  auto page_holder = std::make_unique<DummyPageHolder>(
      IntSize(800, 600), nullptr, nullptr,
      base::BindOnce(&EnableLazyLoadAndEnableDataSaverHoldbackInSettings));
  EXPECT_EQ(LocalFrame::LazyLoadImageSetting::kEnabledExplicit,
            page_holder->GetFrame().GetLazyLoadImageSetting());
}

TEST_F(LocalFrameTest,
       IsLazyLoadingImageAllowedWhenRestrictedWithDataSaverEnabled) {
  ScopedLazyImageLoadingForTest scoped_lazy_image_loading_for_test(true);
  ScopedAutomaticLazyImageLoadingForTest
      scoped_automatic_lazy_image_loading_for_test(true);
  ScopedRestrictAutomaticLazyImageLoadingToDataSaverForTest
      scoped_restrict_automatic_lazy_image_loading_to_data_saver_for_test(true);
  GetNetworkStateNotifier().SetSaveDataEnabled(true);
  auto page_holder = std::make_unique<DummyPageHolder>(
      IntSize(800, 600), nullptr, nullptr,
      base::BindOnce(&EnableLazyLoadAndDisableDataSaverHoldbackInSettings));
  EXPECT_EQ(LocalFrame::LazyLoadImageSetting::kEnabledAutomatic,
            page_holder->GetFrame().GetLazyLoadImageSetting());
}

}  // namespace blink
