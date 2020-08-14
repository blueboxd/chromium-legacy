// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_lacros.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification_platform_bridge_delegate.h"
#include "chromeos/crosapi/mojom/message_center.mojom.h"
#include "chromeos/crosapi/mojom/notification.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/public/cpp/notification.h"
#include "url/gurl.h"

namespace {

// Tracks user actions that would be passed into chrome's cross-platform
// notification subsystem.
class TestPlatformBridgeDelegate : public NotificationPlatformBridgeDelegate {
 public:
  TestPlatformBridgeDelegate() = default;
  TestPlatformBridgeDelegate(const TestPlatformBridgeDelegate&) = delete;
  TestPlatformBridgeDelegate& operator=(const TestPlatformBridgeDelegate&) =
      delete;
  ~TestPlatformBridgeDelegate() override = default;

  // NotificationPlatformBridgeDelegate:
  void HandleNotificationClosed(const std::string& id, bool by_user) override {
    ++closed_count_;
  }
  void HandleNotificationClicked(const std::string& id) override {
    ++clicked_count_;
  }
  void HandleNotificationButtonClicked(
      const std::string& id,
      int button_index,
      const base::Optional<base::string16>& reply) override {
    ++button_clicked_count_;
    last_button_index_ = button_index;
  }
  void HandleNotificationSettingsButtonClicked(const std::string& id) override {
    ++settings_button_clicked_count_;
  }
  void DisableNotification(const std::string& id) override {
    ++disabled_count_;
  }

  // Public because this is test code.
  int closed_count_ = 0;
  int clicked_count_ = 0;
  int button_clicked_count_ = 0;
  int last_button_index_ = -1;
  int settings_button_clicked_count_ = 0;
  int disabled_count_ = 0;
};

// Simulates MessageCenterAsh in ash-chrome.
class TestMessageCenter : public crosapi::mojom::MessageCenter {
 public:
  explicit TestMessageCenter(
      mojo::PendingReceiver<crosapi::mojom::MessageCenter> receiver)
      : receiver_(this, std::move(receiver)) {}
  TestMessageCenter(const TestMessageCenter&) = delete;
  TestMessageCenter& operator=(const TestMessageCenter&) = delete;
  ~TestMessageCenter() override = default;

  void DisplayNotification(
      crosapi::mojom::NotificationPtr notification,
      mojo::PendingRemote<crosapi::mojom::NotificationDelegate> delegate)
      override {
    ++display_count_;
    last_notification_ = std::move(notification);
    // Use unique_ptr to avoid having to deal with unbinding the remote.
    last_notification_delegate_remote_ =
        std::make_unique<mojo::Remote<crosapi::mojom::NotificationDelegate>>(
            std::move(delegate));
  }

  void CloseNotification(const std::string& id) override {
    ++close_count_;
    last_close_id_ = id;
  }

  int display_count_ = 0;
  std::string last_display_id_;
  crosapi::mojom::NotificationPtr last_notification_;
  std::unique_ptr<mojo::Remote<crosapi::mojom::NotificationDelegate>>
      last_notification_delegate_remote_;
  int close_count_ = 0;
  std::string last_close_id_;
  mojo::Receiver<crosapi::mojom::MessageCenter> receiver_;
};

TEST(NotificationPlatformBridgeLacrosTest, Basics) {
  content::BrowserTaskEnvironment task_environment;

  // Create the object under test.
  mojo::Remote<crosapi::mojom::MessageCenter> message_center_remote;
  TestMessageCenter test_message_center(
      message_center_remote.BindNewPipeAndPassReceiver());
  TestPlatformBridgeDelegate bridge_delegate;
  NotificationPlatformBridgeLacros bridge(&bridge_delegate,
                                          &message_center_remote);

  // Create a test notification.
  const base::string16 kTitle = base::ASCIIToUTF16("title");
  const base::string16 kMessage = base::ASCIIToUTF16("message");
  const base::string16 kDisplaySource = base::ASCIIToUTF16("display_source");
  message_center::Notification ui_notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, "test_id", kTitle, kMessage,
      gfx::Image(), kDisplaySource, GURL("http://example.com/"),
      message_center::NotifierId(), {}, nullptr);

  // Show the notification.
  bridge.Display(NotificationHandler::Type::TRANSIENT, /*profile=*/nullptr,
                 ui_notification, /*metadata=*/nullptr);
  message_center_remote.FlushForTesting();
  EXPECT_EQ(1, test_message_center.display_count_);

  // Fields were serialized properly.
  crosapi::mojom::Notification* last_notification =
      test_message_center.last_notification_.get();
  EXPECT_EQ("test_id", last_notification->id);
  EXPECT_EQ(kTitle, last_notification->title);
  EXPECT_EQ(kMessage, last_notification->message);
  EXPECT_EQ(kDisplaySource, last_notification->display_source);
  EXPECT_EQ("http://example.com/", last_notification->origin_url->spec());

  // Grab the mojo::Remote<> for the last notification's delegate.
  ASSERT_TRUE(test_message_center.last_notification_delegate_remote_);
  mojo::Remote<crosapi::mojom::NotificationDelegate>&
      notification_delegate_remote =
          *test_message_center.last_notification_delegate_remote_;

  // Verify remote user actions are forwarded through to |bridge_delegate|.
  notification_delegate_remote->OnNotificationClicked();
  notification_delegate_remote.FlushForTesting();
  EXPECT_EQ(1, bridge_delegate.clicked_count_);

  notification_delegate_remote->OnNotificationButtonClicked(/*button_index=*/0);
  notification_delegate_remote.FlushForTesting();
  EXPECT_EQ(1, bridge_delegate.button_clicked_count_);
  EXPECT_EQ(0, bridge_delegate.last_button_index_);

  notification_delegate_remote->OnNotificationSettingsButtonClicked();
  notification_delegate_remote.FlushForTesting();
  EXPECT_EQ(1, bridge_delegate.settings_button_clicked_count_);

  notification_delegate_remote->OnNotificationDisabled();
  notification_delegate_remote.FlushForTesting();
  EXPECT_EQ(1, bridge_delegate.disabled_count_);

  // Close the notification.
  bridge.Close(/*profile=*/nullptr, "test_id");
  message_center_remote.FlushForTesting();
  EXPECT_EQ(1, test_message_center.close_count_);
  EXPECT_EQ("test_id", test_message_center.last_close_id_);
}

}  // namespace
