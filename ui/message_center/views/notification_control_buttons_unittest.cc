// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_control_buttons_view.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/views/message_view.h"

namespace message_center {

namespace {

class TestMessageView : public MessageView {
 public:
  explicit TestMessageView(const Notification& notification)
      : MessageView(notification),
        buttons_view_(std::make_unique<NotificationControlButtonsView>(this)) {}

  NotificationControlButtonsView* GetControlButtonsView() const override {
    return buttons_view_.get();
  }

 private:
  std::unique_ptr<NotificationControlButtonsView> buttons_view_;
};

}  // namespace

class NotificationControlButtonsTest : public testing::Test {
 public:
  NotificationControlButtonsTest() = default;
  ~NotificationControlButtonsTest() override = default;

  // testing::Test
  void SetUp() override {
    Test::SetUp();
    Notification notification(
        NOTIFICATION_TYPE_SIMPLE, "id", base::UTF8ToUTF16("title"),
        base::UTF8ToUTF16("id"), gfx::Image(), base::string16(), GURL(),
        NotifierId(NotifierType::APPLICATION, "notifier_id"),
        RichNotificationData(), nullptr);
    message_view_ = std::make_unique<TestMessageView>(notification);
  }

  NotificationControlButtonsView* buttons_view() {
    return message_view_->GetControlButtonsView();
  }

 private:
  std::unique_ptr<TestMessageView> message_view_;

  DISALLOW_COPY_AND_ASSIGN(NotificationControlButtonsTest);
};

TEST_F(NotificationControlButtonsTest, TestShowAndHideButtons) {
  EXPECT_EQ(nullptr, buttons_view()->close_button());
  EXPECT_EQ(nullptr, buttons_view()->settings_button());
  EXPECT_EQ(nullptr, buttons_view()->snooze_button());

  buttons_view()->ShowCloseButton(true);
  buttons_view()->ShowSettingsButton(true);
  buttons_view()->ShowSnoozeButton(true);

  EXPECT_NE(nullptr, buttons_view()->close_button());
  EXPECT_NE(nullptr, buttons_view()->settings_button());
  EXPECT_NE(nullptr, buttons_view()->snooze_button());

  buttons_view()->ShowCloseButton(false);
  buttons_view()->ShowSettingsButton(false);
  buttons_view()->ShowSnoozeButton(false);

  EXPECT_EQ(nullptr, buttons_view()->close_button());
  EXPECT_EQ(nullptr, buttons_view()->settings_button());
  EXPECT_EQ(nullptr, buttons_view()->snooze_button());
}

}  // namespace message_center
