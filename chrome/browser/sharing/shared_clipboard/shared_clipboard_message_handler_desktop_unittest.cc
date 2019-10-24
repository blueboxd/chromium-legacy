// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_message_handler_desktop.h"

#include <memory>

#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/notifications/stub_notification_display_service.h"
#include "chrome/browser/sharing/mock_sharing_service.h"
#include "chrome/browser/sharing/proto/shared_clipboard_message.pb.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync_device_info/device_info.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const char kText[] = "clipboard text";
const char kEmptyDeviceName[] = "";
const char kDeviceNameInDeviceInfo[] = "DeviceNameInDeviceInfo";
const char kDeviceNameInMessage[] = "DeviceNameInMessage";

class SharedClipboardMessageHandlerTest : public testing::Test {
 public:
  SharedClipboardMessageHandlerTest() = default;

  void SetUp() override {
    notification_display_service_ =
        std::make_unique<StubNotificationDisplayService>(&profile_);
    sharing_service_ = std::make_unique<MockSharingService>();
    message_handler_ = std::make_unique<SharedClipboardMessageHandlerDesktop>(
        sharing_service_.get(), notification_display_service_.get());
  }

  void TearDown() override {
    ui::Clipboard::DestroyClipboardForCurrentThread();
  }

  chrome_browser_sharing::SharingMessage CreateMessage(
      std::string guid,
      std::string device_name) {
    chrome_browser_sharing::SharingMessage message;
    message.set_sender_guid(guid);
    message.mutable_shared_clipboard_message()->set_text(kText);
    message.set_sender_device_name(device_name);
    return message;
  }

  std::string GetClipboardText() {
    base::string16 text;
    ui::Clipboard::GetForCurrentThread()->ReadText(
        ui::ClipboardBuffer::kCopyPaste, &text);
    return base::UTF16ToUTF8(text);
  }

  message_center::Notification GetNotification() {
    auto notifications =
        notification_display_service_->GetDisplayedNotificationsForType(
            NotificationHandler::Type::SHARING);
    EXPECT_EQ(notifications.size(), 1u);

    const message_center::Notification& notification = notifications[0];
    EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE, notification.type());

    return notification;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<StubNotificationDisplayService> notification_display_service_;
  std::unique_ptr<MockSharingService> sharing_service_;
  std::unique_ptr<SharedClipboardMessageHandlerDesktop> message_handler_;
  TestingProfile profile_;

  DISALLOW_COPY_AND_ASSIGN(SharedClipboardMessageHandlerTest);
};

}  // namespace

TEST_F(SharedClipboardMessageHandlerTest, NotificationWithoutDeviceName) {
  std::string guid = base::GenerateGUID();
  {
    EXPECT_CALL(*sharing_service_, GetDeviceByGuid(guid))
        .WillOnce(
            [](const std::string& guid) -> std::unique_ptr<syncer::DeviceInfo> {
              return nullptr;
            });
    base::MockCallback<SharingMessageHandler::DoneCallback> done_callback;
    EXPECT_CALL(done_callback, Run(testing::Eq(nullptr))).Times(1);
    message_handler_->OnMessage(CreateMessage(guid, kEmptyDeviceName),
                                done_callback.Get());
  }
  EXPECT_EQ(GetClipboardText(), kText);

  message_center::Notification notification = GetNotification();
  EXPECT_EQ(
      l10n_util::GetStringUTF16(
          IDS_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_NOTIFICATION_TITLE_UNKNOWN_DEVICE),
      notification.title());
}

TEST_F(SharedClipboardMessageHandlerTest,
       NotificationWithDeviceNameFromDeviceInfo) {
  std::string guid = base::GenerateGUID();
  {
    EXPECT_CALL(*sharing_service_, GetDeviceByGuid(guid))
        .WillOnce(
            [](const std::string& guid) -> std::unique_ptr<syncer::DeviceInfo> {
              return std::make_unique<syncer::DeviceInfo>(
                  base::GenerateGUID(), kDeviceNameInDeviceInfo,
                  /*chrome_version=*/"78.0.0.0",
                  /*sync_user_agent=*/"Chrome", sync_pb::SyncEnums::TYPE_LINUX,
                  /*signin_scoped_device_id=*/base::GenerateGUID(),
                  base::SysInfo::HardwareInfo(),
                  /*last_updated_timestamp=*/base::Time::Now(),
                  /*send_tab_to_self_receiving_enabled=*/false,
                  /*sharing_info=*/base::nullopt);
            });
    base::MockCallback<SharingMessageHandler::DoneCallback> done_callback;
    EXPECT_CALL(done_callback, Run(testing::Eq(nullptr))).Times(1);
    message_handler_->OnMessage(CreateMessage(guid, kEmptyDeviceName),
                                done_callback.Get());
  }
  message_center::Notification notification = GetNotification();
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_NOTIFICATION_TITLE,
                base::ASCIIToUTF16(kDeviceNameInDeviceInfo)),
            notification.title());
}

TEST_F(SharedClipboardMessageHandlerTest,
       NotificationWithDeviceNameFromMessage) {
  std::string guid = base::GenerateGUID();
  {
    EXPECT_CALL(*sharing_service_, GetDeviceByGuid(guid))
        .WillOnce(
            [](const std::string& guid) -> std::unique_ptr<syncer::DeviceInfo> {
              return nullptr;
            });
    base::MockCallback<SharingMessageHandler::DoneCallback> done_callback;
    EXPECT_CALL(done_callback, Run(testing::Eq(nullptr))).Times(1);
    message_handler_->OnMessage(CreateMessage(guid, kDeviceNameInMessage),
                                done_callback.Get());
  }
  EXPECT_EQ(GetClipboardText(), kText);

  message_center::Notification notification = GetNotification();
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_NOTIFICATION_TITLE,
                base::ASCIIToUTF16(kDeviceNameInMessage)),
            notification.title());
}
