// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing/sharing_dialog_view.h"

#include <map>
#include <memory>
#include <string>

#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_ui_controller.h"
#include "chrome/browser/sharing/sharing_app.h"
#include "chrome/browser/sharing/sharing_metrics.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "components/sync_device_info/device_info.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_utils.h"

using namespace testing;

namespace {

class ClickToCallUiControllerMock : public ClickToCallUiController {
 public:
  explicit ClickToCallUiControllerMock(content::WebContents* web_contents)
      : ClickToCallUiController(web_contents) {}
  ~ClickToCallUiControllerMock() override = default;

  MOCK_METHOD1(OnDeviceChosen, void(const syncer::DeviceInfo& device));
  MOCK_METHOD1(OnAppChosen, void(const SharingApp& app));
  MOCK_METHOD1(OnHelpTextClicked, void(SharingDialogType dialog_type));
};

class SharingDialogViewMock : public SharingDialogView {
 public:
  SharingDialogViewMock(views::View* anchor_view,
                        content::WebContents* web_contents,
                        ClickToCallUiController* controller)
      : SharingDialogView(anchor_view, web_contents, controller) {}
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

    profile_ = std::make_unique<TestingProfile>();
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr);

    // Create an anchor for the bubble.
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    anchor_widget_ = std::make_unique<views::Widget>();
    anchor_widget_->Init(std::move(params));

    controller_ =
        std::make_unique<ClickToCallUiControllerMock>(web_contents_.get());
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

  std::unique_ptr<SharingDialogView> CreateDialogView(int devices, int apps) {
    controller_->set_devices_for_testing(CreateDevices(devices));
    controller_->set_apps_for_testing(CreateApps(apps));

    auto dialog = std::make_unique<SharingDialogViewMock>(
        anchor_widget_->GetContentsView(), /*web_contents=*/nullptr,
        controller_.get());
    dialog->Init();
    return dialog;
  }

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<views::Widget> anchor_widget_;
  std::unique_ptr<ClickToCallUiControllerMock> controller_;
};

TEST_F(SharingDialogViewTest, PopulateDialogView) {
  auto dialog = CreateDialogView(/*devices=*/3, /*apps=*/2);

  EXPECT_EQ(5UL, dialog->dialog_buttons_.size());
}

TEST_F(SharingDialogViewTest, DevicePressed) {
  syncer::DeviceInfo device_info(
      "device_guid_1", "device1", "chrome_version", "user_agent",
      sync_pb::SyncEnums_DeviceType_TYPE_PHONE, "device_id",
      /*last_updated_timestamp=*/base::Time::Now(),
      /*send_tab_to_self_receiving_enabled=*/false,
      /*sharing_info=*/base::nullopt);
  EXPECT_CALL(*controller_.get(), OnDeviceChosen(DeviceEquals(&device_info)));

  auto dialog = CreateDialogView(/*devices=*/3, /*apps=*/2);

  const ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), 0, 0);

  // Choose second device: device0(tag=0), device1(tag=1)
  dialog->ButtonPressed(dialog->dialog_buttons_[1], event);
}

TEST_F(SharingDialogViewTest, AppPressed) {
  SharingApp app(&vector_icons::kOpenInNewIcon, gfx::Image(),
                 base::UTF8ToUTF16("app0"), std::string());
  EXPECT_CALL(*controller_.get(), OnAppChosen(AppEquals(&app)));

  auto dialog = CreateDialogView(/*devices=*/3, /*apps=*/2);

  const ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), 0, 0);

  // Choose first app: device0(tag=0), device1(tag=1), device2(tag=2),
  // app0(tag=3)
  dialog->ButtonPressed(dialog->dialog_buttons_[3], event);
}

TEST_F(SharingDialogViewTest, HelpTextClickedEmpty) {
  EXPECT_CALL(*controller_.get(),
              OnHelpTextClicked(SharingDialogType::kEducationalDialog));

  auto dialog = CreateDialogView(/*devices=*/0, /*apps=*/0);

  dialog->StyledLabelLinkClicked(/*label=*/nullptr, /*range=*/{},
                                 /*event_flags=*/0);
}

TEST_F(SharingDialogViewTest, HelpTextClickedOnlyApps) {
  EXPECT_CALL(
      *controller_.get(),
      OnHelpTextClicked(SharingDialogType::kDialogWithoutDevicesWithApp));

  auto dialog = CreateDialogView(/*devices=*/0, /*apps=*/1);

  dialog->StyledLabelLinkClicked(/*label=*/nullptr, /*range=*/{},
                                 /*event_flags=*/0);
}

TEST_F(SharingDialogViewTest, ThemeChangedEmptyList) {
  controller_->set_send_result_for_testing(
      SharingSendMessageResult::kDeviceNotFound);
  auto dialog = CreateDialogView(/*devices=*/1, /*apps=*/1);

  EXPECT_EQ(SharingDialogType::kErrorDialog, dialog->GetDialogType());

  // Regression test for crbug.com/1001112
  dialog->OnThemeChanged();
}
