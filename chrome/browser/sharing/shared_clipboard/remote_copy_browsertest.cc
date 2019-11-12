// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/guid.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharing/shared_clipboard/feature_flags.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_fcm_handler.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"

namespace {
const char kText[] = "clipboard text";
const char kDeviceName[] = "DeviceName";
}  // namespace

// Browser tests for the Remote Copy feature.
class RemoteCopyBrowserTestBase : public InProcessBrowserTest {
 public:
  RemoteCopyBrowserTestBase() = default;
  ~RemoteCopyBrowserTestBase() override = default;

  void SetUpOnMainThread() override {
    ui::TestClipboard::CreateForCurrentThread();
    notification_tester_ = std::make_unique<NotificationDisplayServiceTester>(
        browser()->profile());
    sharing_service_ =
        SharingServiceFactory::GetForBrowserContext(browser()->profile());
  }

  void TearDownOnMainThread() override {
    notification_tester_.reset();
    ui::Clipboard::DestroyClipboardForCurrentThread();
  }

  gcm::IncomingMessage CreateMessage(std::string guid,
                                     std::string device_name,
                                     std::string text) {
    chrome_browser_sharing::SharingMessage sharing_message;
    sharing_message.set_sender_guid(guid);
    sharing_message.set_sender_device_name(device_name);
    sharing_message.mutable_remote_copy_message()->set_text(text);

    gcm::IncomingMessage incoming_message;
    std::string serialized_sharing_message;
    sharing_message.SerializeToString(&serialized_sharing_message);
    incoming_message.raw_data = serialized_sharing_message;
    return incoming_message;
  }

  std::string GetClipboardText() {
    base::string16 text;
    ui::Clipboard::GetForCurrentThread()->ReadText(
        ui::ClipboardBuffer::kCopyPaste, &text);
    return base::UTF16ToUTF8(text);
  }

  message_center::Notification GetNotification() {
    auto notifications = notification_tester_->GetDisplayedNotificationsForType(
        NotificationHandler::Type::SHARING);
    EXPECT_EQ(notifications.size(), 1u);

    const message_center::Notification& notification = notifications[0];
    EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE, notification.type());

    return notification;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<NotificationDisplayServiceTester> notification_tester_;
  SharingService* sharing_service_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RemoteCopyBrowserTestBase);
};

class RemoteCopyBrowserTest : public RemoteCopyBrowserTestBase {
 public:
  RemoteCopyBrowserTest() {
    feature_list_.InitWithFeatures({kRemoteCopyReceiver}, {});
  }
};

IN_PROC_BROWSER_TEST_F(RemoteCopyBrowserTest, HandleMessage) {
  sharing_service_->GetFCMHandlerForTesting()->OnMessage(
      kSharingFCMAppID,
      CreateMessage(base::GenerateGUID(), kDeviceName, kText));
  EXPECT_EQ(GetClipboardText(), kText);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_NOTIFICATION_TITLE,
                base::ASCIIToUTF16(kDeviceName)),
            GetNotification().title());
}
