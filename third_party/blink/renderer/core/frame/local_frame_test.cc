// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/local_frame.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

namespace {

void DisableLazyLoadInSettings(Settings& settings) {
  settings.SetLazyLoadEnabled(false);
}
void EnableLazyLoadInSettings(Settings& settings) {
  settings.SetLazyLoadEnabled(true);
}

}  // namespace

class LocalFrameTest : public testing::Test {
 public:
  void TearDown() override {
    // Reset the global data saver setting to false at the end of the test.
    GetNetworkStateNotifier().SetSaveDataEnabled(false);
  }
};

TEST_F(LocalFrameTest, IsLazyLoadingImageAllowedWithFeatureDisabled) {
  ScopedLazyImageLoadingForTest scoped_lazy_image_loading_for_test(false);
  auto page_holder = std::make_unique<DummyPageHolder>(
      IntSize(800, 600), nullptr, nullptr,
      base::BindOnce(&EnableLazyLoadInSettings));
  EXPECT_EQ(LocalFrame::LazyLoadImageSetting::kDisabled,
            page_holder->GetFrame().GetLazyLoadImageSetting());
}

TEST_F(LocalFrameTest, IsLazyLoadingImageAllowedWithSettingDisabled) {
  ScopedLazyImageLoadingForTest scoped_lazy_image_loading_for_test(false);
  auto page_holder = std::make_unique<DummyPageHolder>(
      IntSize(800, 600), nullptr, nullptr,
      base::BindOnce(&DisableLazyLoadInSettings));
  EXPECT_EQ(LocalFrame::LazyLoadImageSetting::kDisabled,
            page_holder->GetFrame().GetLazyLoadImageSetting());
}

TEST_F(LocalFrameTest, IsLazyLoadingImageAllowedWithAutomaticDisabled) {
  ScopedLazyImageLoadingForTest scoped_lazy_image_loading_for_test(true);
  ScopedAutomaticLazyImageLoadingForTest
      scoped_automatic_lazy_image_loading_for_test(false);
  auto page_holder = std::make_unique<DummyPageHolder>(
      IntSize(800, 600), nullptr, nullptr,
      base::BindOnce(&EnableLazyLoadInSettings));
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
      base::BindOnce(&EnableLazyLoadInSettings));
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
      base::BindOnce(&EnableLazyLoadInSettings));
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
      base::BindOnce(&EnableLazyLoadInSettings));
  EXPECT_EQ(LocalFrame::LazyLoadImageSetting::kEnabledAutomatic,
            page_holder->GetFrame().GetLazyLoadImageSetting());
}

namespace {

void TestGreenDiv(DummyPageHolder& page_holder) {
  const Document& doc = page_holder.GetDocument();
  Element* div = doc.getElementById("div");
  ASSERT_TRUE(div);
  ASSERT_TRUE(div->GetComputedStyle());
  EXPECT_EQ(MakeRGB(0, 128, 0), div->GetComputedStyle()->VisitedDependentColor(
                                    GetCSSPropertyColor()));
}

}  // namespace

TEST_F(LocalFrameTest, ForceSynchronousDocumentInstall_XHTMLStyleInBody) {
  auto page_holder = std::make_unique<DummyPageHolder>(IntSize(800, 600));

  scoped_refptr<SharedBuffer> data = SharedBuffer::Create();
  data->Append(
      "<html xmlns='http://www.w3.org/1999/xhtml'><body><style>div { color: "
      "green }</style><div id='div'></div></body></html>",
      static_cast<size_t>(118));
  page_holder->GetFrame().ForceSynchronousDocumentInstall("text/xml", data);
  TestGreenDiv(*page_holder);
}

TEST_F(LocalFrameTest, ForceSynchronousDocumentInstall_XHTMLLinkInBody) {
  auto page_holder = std::make_unique<DummyPageHolder>(IntSize(800, 600));

  scoped_refptr<SharedBuffer> data = SharedBuffer::Create();
  data->Append(
      "<html xmlns='http://www.w3.org/1999/xhtml'><body><link rel='stylesheet' "
      "href='data:text/css,div{color:green}' /><div "
      "id='div'></div></body></html>",
      static_cast<size_t>(146));
  page_holder->GetFrame().ForceSynchronousDocumentInstall("text/xml", data);
  TestGreenDiv(*page_holder);
}

TEST_F(LocalFrameTest, ForceSynchronousDocumentInstall_XHTMLStyleInHead) {
  auto page_holder = std::make_unique<DummyPageHolder>(IntSize(800, 600));

  scoped_refptr<SharedBuffer> data = SharedBuffer::Create();
  data->Append(
      "<html xmlns='http://www.w3.org/1999/xhtml'><head><style>div { color: "
      "green }</style></head><body><div id='div'></div></body></html>",
      static_cast<size_t>(131));
  page_holder->GetFrame().ForceSynchronousDocumentInstall("text/xml", data);
  TestGreenDiv(*page_holder);
}

TEST_F(LocalFrameTest, ForceSynchronousDocumentInstall_XHTMLLinkInHead) {
  auto page_holder = std::make_unique<DummyPageHolder>(IntSize(800, 600));

  scoped_refptr<SharedBuffer> data = SharedBuffer::Create();
  data->Append(
      "<html xmlns='http://www.w3.org/1999/xhtml'><head><link rel='stylesheet' "
      "href='data:text/css,div{color:green}' /></head><body><div "
      "id='div'></div></body></html>",
      static_cast<size_t>(159));
  page_holder->GetFrame().ForceSynchronousDocumentInstall("text/xml", data);
  TestGreenDiv(*page_holder);
}

TEST_F(LocalFrameTest, ForceSynchronousDocumentInstall_XMLStyleSheet) {
  auto page_holder = std::make_unique<DummyPageHolder>(IntSize(800, 600));

  scoped_refptr<SharedBuffer> data = SharedBuffer::Create();
  data->Append(
      "<?xml-stylesheet type='text/css' "
      "href='data:text/css,div{color:green}'?><html "
      "xmlns='http://www.w3.org/1999/xhtml'><body><div "
      "id='div'></div></body></html>",
      static_cast<size_t>(155));
  page_holder->GetFrame().ForceSynchronousDocumentInstall("text/xml", data);
  TestGreenDiv(*page_holder);
}

}  // namespace blink
