// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/message_center_ash.h"

#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chromeos/crosapi/mojom/message_center.mojom.h"
#include "chromeos/crosapi/mojom/notification.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "url/gurl.h"

namespace crosapi {
namespace {

class MojoDelegate : public mojom::NotificationDelegate {
 public:
  MojoDelegate() = default;
  MojoDelegate(const MojoDelegate&) = delete;
  MojoDelegate& operator=(const MojoDelegate&) = delete;
  ~MojoDelegate() override = default;

  // crosapi::mojom::NotificationDelegate:
  void OnNotificationClosed(bool by_user) override { ++closed_count_; }
  void OnNotificationClicked() override { ++clicked_count_; }
  void OnNotificationButtonClicked(uint32_t button_index) override {
    ++button_clicked_count_;
    last_button_index_ = button_index;
  }
  void OnNotificationSettingsButtonClicked() override {
    ++settings_button_clicked_count_;
  }
  void OnNotificationDisabled() override { ++disabled_count_; }

  // Public because this is test code.
  int closed_count_ = 0;
  int clicked_count_ = 0;
  int button_clicked_count_ = 0;
  uint32_t last_button_index_ = 0;
  int settings_button_clicked_count_ = 0;
  int disabled_count_ = 0;
  mojo::Receiver<mojom::NotificationDelegate> receiver_{this};
};

class MessageCenterAshTest : public testing::Test {
 public:
  MessageCenterAshTest() = default;
  MessageCenterAshTest(const MessageCenterAshTest&) = delete;
  MessageCenterAshTest& operator=(const MessageCenterAshTest&) = delete;
  ~MessageCenterAshTest() override = default;

  // testing::Test:
  void SetUp() override { message_center::MessageCenter::Initialize(); }

  void TearDown() override { message_center::MessageCenter::Shutdown(); }

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(MessageCenterAshTest, Basics) {
  // Create object under test.
  mojo::Remote<mojom::MessageCenter> remote;
  MessageCenterAsh message_center_ash(remote.BindNewPipeAndPassReceiver());

  // Build mojo notification for display.
  auto mojo_notification = mojom::Notification::New();
  mojo_notification->type = mojom::NotificationType::kSimple;
  mojo_notification->id = "test_id";
  mojo_notification->title = base::ASCIIToUTF16("title");
  mojo_notification->message = base::ASCIIToUTF16("message");
  mojo_notification->display_source = base::ASCIIToUTF16("source");
  mojo_notification->origin_url = GURL("http://example.com/");

  // Display the notification.
  MojoDelegate mojo_delegate;
  remote->DisplayNotification(
      std::move(mojo_notification),
      mojo_delegate.receiver_.BindNewPipeAndPassRemote());
  remote.FlushForTesting();

  // Notification exists and has correct fields.
  auto* message_center = message_center::MessageCenter::Get();
  message_center::Notification* ui_notification =
      message_center->FindVisibleNotificationById("test_id");
  ASSERT_TRUE(ui_notification);
  EXPECT_EQ("test_id", ui_notification->id());
  EXPECT_EQ(base::ASCIIToUTF16("title"), ui_notification->title());
  EXPECT_EQ(base::ASCIIToUTF16("message"), ui_notification->message());
  EXPECT_EQ(base::ASCIIToUTF16("source"), ui_notification->display_source());
  EXPECT_EQ("http://example.com/", ui_notification->origin_url().spec());

  // Simulate the user clicking on the notification body.
  ui_notification->delegate()->Click(/*button_index=*/base::nullopt,
                                     /*reply=*/base::nullopt);
  mojo_delegate.receiver_.FlushForTesting();
  EXPECT_EQ(1, mojo_delegate.clicked_count_);

  // Simulate the user clicking on a notification button.
  ui_notification->delegate()->Click(/*button_index=*/1,
                                     /*reply=*/base::nullopt);
  mojo_delegate.receiver_.FlushForTesting();
  EXPECT_EQ(1, mojo_delegate.button_clicked_count_);
  EXPECT_EQ(1u, mojo_delegate.last_button_index_);

  // Simulate the user clicking on the settings button.
  ui_notification->delegate()->SettingsClick();
  mojo_delegate.receiver_.FlushForTesting();
  EXPECT_EQ(1, mojo_delegate.settings_button_clicked_count_);

  // Simulate the user disabling this notification.
  ui_notification->delegate()->DisableNotification();
  mojo_delegate.receiver_.FlushForTesting();
  EXPECT_EQ(1, mojo_delegate.disabled_count_);

  // Close the notification.
  remote->CloseNotification("test_id");
  remote.FlushForTesting();
  EXPECT_FALSE(message_center->FindVisibleNotificationById("test_id"));
  EXPECT_EQ(1, mojo_delegate.closed_count_);
}

}  // namespace
}  // namespace crosapi
