// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing/sharing_dialog_view.h"

#include <memory>
#include <string>

#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/sharing/sharing_app.h"
#include "chrome/browser/sharing/sharing_metrics.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/sync_device_info/device_info.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_utils.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/test/widget_test.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

class SharingDialogViewMock : public SharingDialogView {
 public:
  SharingDialogViewMock(views::View* anchor_view,
                        content::WebContents* web_contents,
                        SharingDialogData data)
      : SharingDialogView(anchor_view, web_contents, std::move(data)) {}
  ~SharingDialogViewMock() override = default;

  // The delegate cannot find widget since it is created from a null profile.
  // This method will be called inside ButtonPressed(). Unit tests will
  // crash without mocking.
  MOCK_METHOD0(CloseBubble, void());
};

}  // namespace

MATCHER_P(DeviceEquals, device, "") {
  return device->guid() == arg.guid();
}

MATCHER_P(AppEquals, app, "") {
  return app->name == arg.name;
}

class SharingDialogViewTest : public ChromeViewsTestBase {
 protected:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    // Create an anchor for the bubble.
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    anchor_widget_ = std::make_unique<views::Widget>();
    anchor_widget_->Init(std::move(params));
  }

  void TearDown() override {
    anchor_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  std::vector<std::unique_ptr<syncer::DeviceInfo>> CreateDevices(int count) {
    std::vector<std::unique_ptr<syncer::DeviceInfo>> devices;
    for (int i = 0; i < count; i++) {
      devices.emplace_back(std::make_unique<syncer::DeviceInfo>(
          base::StrCat({"device_guid_", base::NumberToString(i)}),
          base::StrCat({"device", base::NumberToString(i)}), "chrome_version",
          "user_agent", sync_pb::SyncEnums_DeviceType_TYPE_PHONE, "device_id",
          base::SysInfo::HardwareInfo(),
          /*last_updated_timestamp=*/base::Time::Now(),
          /*send_tab_to_self_receiving_enabled=*/false,
          /*sharing_info=*/base::nullopt));
    }
    return devices;
  }

  std::vector<SharingApp> CreateApps(int count) {
    std::vector<SharingApp> apps;
    for (int i = 0; i < count; i++) {
      apps.emplace_back(
          &vector_icons::kOpenInNewIcon, gfx::Image(),
          base::UTF8ToUTF16(base::StrCat({"app", base::NumberToString(i)})),
          base::StrCat({"app_id_", base::NumberToString(i)}));
    }
    return apps;
  }

  std::unique_ptr<SharingDialogView> CreateDialogView(
      SharingDialogData dialog_data) {
    auto dialog = std::make_unique<SharingDialogViewMock>(
        anchor_widget_->GetContentsView(), /*web_contents=*/nullptr,
        std::move(dialog_data));
    dialog->Init();
    return dialog;
  }

  SharingDialogData CreateDialogData(int devices, int apps) {
    SharingDialogData data;

    if (devices)
      data.type = SharingDialogType::kDialogWithDevicesMaybeApps;
    else if (apps)
      data.type = SharingDialogType::kDialogWithoutDevicesWithApp;
    else
      data.type = SharingDialogType::kEducationalDialog;

    data.prefix = SharingFeatureName::kClickToCall;
    data.devices = CreateDevices(devices);
    data.apps = CreateApps(apps);

    data.help_text_id =
        IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_HELP_TEXT_NO_DEVICES;
    data.help_link_text_id =
        IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_TROUBLESHOOT_LINK;

    data.help_callback = base::BindLambdaForTesting(
        [&](SharingDialogType type) { help_callback.Call(type); });
    data.device_callback =
        base::BindLambdaForTesting([&](const syncer::DeviceInfo& device) {
          device_callback.Call(device);
        });
    data.app_callback = base::BindLambdaForTesting(
        [&](const SharingApp& app) { app_callback.Call(app); });

    return data;
  }

  std::unique_ptr<views::Widget> anchor_widget_;
  testing::MockFunction<void(SharingDialogType)> help_callback;
  testing::MockFunction<void(const syncer::DeviceInfo&)> device_callback;
  testing::MockFunction<void(const SharingApp&)> app_callback;
};

TEST_F(SharingDialogViewTest, PopulateDialogView) {
  auto dialog_data = CreateDialogData(/*devices=*/3, /*apps=*/2);
  auto dialog = CreateDialogView(std::move(dialog_data));

  EXPECT_EQ(5UL, dialog->dialog_buttons_.size());
}

TEST_F(SharingDialogViewTest, DevicePressed) {
  syncer::DeviceInfo device_info("device_guid_1", "device1", "chrome_version",
                                 "user_agent",
                                 sync_pb::SyncEnums_DeviceType_TYPE_PHONE,
                                 "device_id", base::SysInfo::HardwareInfo(),
                                 /*last_updated_timestamp=*/base::Time::Now(),
                                 /*send_tab_to_self_receiving_enabled=*/false,
                                 /*sharing_info=*/base::nullopt);
  EXPECT_CALL(device_callback, Call(DeviceEquals(&device_info)));

  auto dialog_data = CreateDialogData(/*devices=*/3, /*apps=*/2);
  auto dialog = CreateDialogView(std::move(dialog_data));

  const ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), 0, 0);

  // Choose second device: device0(tag=0), device1(tag=1)
  dialog->ButtonPressed(dialog->dialog_buttons_[1], event);
}

TEST_F(SharingDialogViewTest, AppPressed) {
  SharingApp app(&vector_icons::kOpenInNewIcon, gfx::Image(),
                 base::UTF8ToUTF16("app0"), std::string());
  EXPECT_CALL(app_callback, Call(AppEquals(&app)));

  auto dialog_data = CreateDialogData(/*devices=*/3, /*apps=*/2);
  auto dialog = CreateDialogView(std::move(dialog_data));

  const ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), 0, 0);

  // Choose first app: device0(tag=0), device1(tag=1), device2(tag=2),
  // app0(tag=3)
  dialog->ButtonPressed(dialog->dialog_buttons_[3], event);
}

TEST_F(SharingDialogViewTest, HelpTextClickedEmpty) {
  EXPECT_CALL(help_callback, Call(SharingDialogType::kEducationalDialog));

  auto dialog_data = CreateDialogData(/*devices=*/0, /*apps=*/0);
  auto dialog = CreateDialogView(std::move(dialog_data));

  dialog->StyledLabelLinkClicked(/*label=*/nullptr, /*range=*/{},
                                 /*event_flags=*/0);
}

TEST_F(SharingDialogViewTest, HelpTextClickedOnlyApps) {
  EXPECT_CALL(help_callback,
              Call(SharingDialogType::kDialogWithoutDevicesWithApp));

  auto dialog_data = CreateDialogData(/*devices=*/0, /*apps=*/1);
  auto dialog = CreateDialogView(std::move(dialog_data));

  dialog->StyledLabelLinkClicked(/*label=*/nullptr, /*range=*/{},
                                 /*event_flags=*/0);
}

TEST_F(SharingDialogViewTest, ThemeChangedEmptyList) {
  auto dialog_data = CreateDialogData(/*devices=*/1, /*apps=*/1);
  dialog_data.type = SharingDialogType::kErrorDialog;
  auto dialog = CreateDialogView(std::move(dialog_data));

  EXPECT_EQ(SharingDialogType::kErrorDialog, dialog->GetDialogType());

  // Regression test for crbug.com/1001112
  dialog->OnThemeChanged();
}

TEST_F(SharingDialogViewTest, OriginViewShown) {
  auto dialog_data = CreateDialogData(/*devices=*/1, /*apps=*/1);
  dialog_data.origin_text_id =
      IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_INITIATING_ORIGIN;
  dialog_data.initiating_origin =
      url::Origin::Create(GURL("https://example.com"));

  views::test::WidgetTest::WidgetAutoclosePtr bubble_widget(
      views::BubbleDialogDelegateView::CreateBubble(
          CreateDialogView(std::move(dialog_data)).release()));

  auto* frame_view = static_cast<views::BubbleFrameView*>(
      bubble_widget->non_client_view()->frame_view());

  EXPECT_NE(nullptr, frame_view->GetHeaderViewForTesting());
}

TEST_F(SharingDialogViewTest, OriginViewHiddenIfNoOrigin) {
  auto dialog_data = CreateDialogData(/*devices=*/1, /*apps=*/1);
  dialog_data.origin_text_id =
      IDS_BROWSER_SHARING_CLICK_TO_CALL_DIALOG_INITIATING_ORIGIN;
  // Do not set an origin.
  dialog_data.initiating_origin = base::nullopt;

  views::test::WidgetTest::WidgetAutoclosePtr bubble_widget(
      views::BubbleDialogDelegateView::CreateBubble(
          CreateDialogView(std::move(dialog_data)).release()));

  auto* frame_view = static_cast<views::BubbleFrameView*>(
      bubble_widget->non_client_view()->frame_view());

  EXPECT_EQ(nullptr, frame_view->GetHeaderViewForTesting());
}

TEST_F(SharingDialogViewTest, OriginViewHiddenIfNoOriginText) {
  auto dialog_data = CreateDialogData(/*devices=*/1, /*apps=*/1);
  // Do not set an origin text.
  dialog_data.origin_text_id = 0;
  dialog_data.initiating_origin =
      url::Origin::Create(GURL("https://example.com"));

  views::test::WidgetTest::WidgetAutoclosePtr bubble_widget(
      views::BubbleDialogDelegateView::CreateBubble(
          CreateDialogView(std::move(dialog_data)).release()));

  auto* frame_view = static_cast<views::BubbleFrameView*>(
      bubble_widget->non_client_view()->frame_view());

  EXPECT_EQ(nullptr, frame_view->GetHeaderViewForTesting());
}
