// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing/click_to_call/click_to_call_dialog_view.h"

#include <map>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_utils.h"

using namespace testing;

namespace {

class ClickToCallSharingDialogControllerMock
    : public ClickToCallSharingDialogController {
 public:
  explicit ClickToCallSharingDialogControllerMock(
      content::WebContents* web_contents)
      : ClickToCallSharingDialogController(web_contents) {}
  ~ClickToCallSharingDialogControllerMock() override = default;

  MOCK_METHOD1(OnDeviceChosen, void(const SharingDeviceInfo& device));
  MOCK_METHOD1(OnAppChosen, void(const App& app));

  MOCK_METHOD0(GetSyncedDevices, std::vector<SharingDeviceInfo>());

  MOCK_METHOD0(GetApps, std::vector<App>());
};

class ClickToCallDialogViewMock : public ClickToCallDialogView {
 public:
  ClickToCallDialogViewMock(views::View* anchor_view,
                            content::WebContents* web_contents,
                            ClickToCallSharingDialogController* controller)
      : ClickToCallDialogView(anchor_view, web_contents, controller) {}
  ~ClickToCallDialogViewMock() override = default;

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

class ClickToCallDialogViewTest : public ChromeViewsTestBase {
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
    anchor_widget_->Init(params);

    controller_ = std::make_unique<ClickToCallSharingDialogControllerMock>(
        web_contents_.get());

    devices_ = SetUpDevices();
    apps_ = SetUpApps();
  }

  void TearDown() override {
    anchor_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  std::vector<SharingDeviceInfo> SetUpDevices() {
    std::vector<SharingDeviceInfo> devices;
    devices.emplace_back("device_guid_1", base::UTF8ToUTF16("device1"),
                         sync_pb::SyncEnums::TYPE_PHONE, base::Time::Now(), 1);
    devices.emplace_back("device_guid_2", base::UTF8ToUTF16("device2"),
                         sync_pb::SyncEnums::TYPE_PHONE, base::Time::Now(), 1);
    devices.emplace_back("device_guid_3", base::UTF8ToUTF16("device3"),
                         sync_pb::SyncEnums::TYPE_PHONE, base::Time::Now(), 1);
    return devices;
  }

  std::vector<ClickToCallSharingDialogController::App> SetUpApps() {
    std::vector<ClickToCallSharingDialogController::App> apps;
    apps.emplace_back(vector_icons::kOpenInNewIcon, base::UTF8ToUTF16("app_1"),
                      std::string());
    apps.emplace_back(vector_icons::kOpenInNewIcon, base::UTF8ToUTF16("app_2"),
                      std::string());
    return apps;
  }

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<views::Widget> anchor_widget_;
  std::unique_ptr<ClickToCallSharingDialogControllerMock> controller_;
  std::vector<SharingDeviceInfo> devices_;
  std::vector<ClickToCallSharingDialogController::App> apps_;
};

TEST_F(ClickToCallDialogViewTest, PopulateDialogView) {
  EXPECT_CALL(*controller_.get(), GetSyncedDevices())
      .WillOnce(Return(ByMove(std::move(devices_))));
  EXPECT_CALL(*controller_.get(), GetApps())
      .WillOnce(Return(ByMove(std::move(apps_))));
  std::unique_ptr<ClickToCallDialogView> bubble_view_ =
      std::make_unique<ClickToCallDialogViewMock>(
          anchor_widget_->GetContentsView(), nullptr, controller_.get());

  bubble_view_->Init();
  EXPECT_EQ(5UL, bubble_view_->dialog_buttons_.size());
}

TEST_F(ClickToCallDialogViewTest, DevicePressed) {
  SharingDeviceInfo sharing_device_info(
      "device_guid_2", base::UTF8ToUTF16("device2"),
      sync_pb::SyncEnums::TYPE_PHONE, base::Time::Now(), 1);
  EXPECT_CALL(*controller_.get(), GetSyncedDevices())
      .WillOnce(Return(ByMove(std::move(devices_))));
  EXPECT_CALL(*controller_.get(), GetApps())
      .WillOnce(Return(ByMove(std::move(apps_))));
  EXPECT_CALL(*controller_.get(),
              OnDeviceChosen(DeviceEquals(&sharing_device_info)));

  std::unique_ptr<ClickToCallDialogView> bubble_view_ =
      std::make_unique<ClickToCallDialogViewMock>(
          anchor_widget_->GetContentsView(), nullptr, controller_.get());

  bubble_view_->Init();

  const ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), 0, 0);

  // Choose second device: device1(tag=0), device2(tag=1)
  bubble_view_->ButtonPressed(bubble_view_->dialog_buttons_[1], event);
}

TEST_F(ClickToCallDialogViewTest, AppPressed) {
  ClickToCallSharingDialogController::App app(
      vector_icons::kOpenInNewIcon, base::UTF8ToUTF16("app_1"), std::string());
  EXPECT_CALL(*controller_.get(), GetSyncedDevices())
      .WillOnce(Return(ByMove(std::move(devices_))));
  EXPECT_CALL(*controller_.get(), GetApps())
      .WillOnce(Return(ByMove(std::move(apps_))));
  EXPECT_CALL(*controller_.get(), OnAppChosen(AppEquals(&app)));

  std::unique_ptr<ClickToCallDialogView> bubble_view_ =
      std::make_unique<ClickToCallDialogViewMock>(
          anchor_widget_->GetContentsView(), nullptr, controller_.get());

  bubble_view_->Init();

  const ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), 0, 0);

  // Choose first app: device1(tag=0), device2(tag=1), device3(tag=2),
  // app1(tag=3)
  bubble_view_->ButtonPressed(bubble_view_->dialog_buttons_[3], event);
}
