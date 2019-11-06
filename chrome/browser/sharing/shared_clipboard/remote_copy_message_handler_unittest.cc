// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/shared_clipboard/remote_copy_message_handler.h"

#include "base/bind_helpers.h"
#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_test_base.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync/protocol/sharing_message.pb.h"
#include "components/sync/protocol/sharing_remote_copy_message.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"

namespace {

const char kText[] = "clipboard text";
const char kEmptyDeviceName[] = "";
const char kDeviceNameInMessage[] = "DeviceNameInMessage";

class RemoteCopyMessageHandlerTest : public SharedClipboardTestBase {
 public:
  RemoteCopyMessageHandlerTest() = default;

  ~RemoteCopyMessageHandlerTest() override = default;

  void SetUp() override {
    SharedClipboardTestBase::SetUp();
    message_handler_ = std::make_unique<RemoteCopyMessageHandler>(&profile_);
  }

  chrome_browser_sharing::SharingMessage CreateMessage(std::string guid,
                                                       std::string device_name,
                                                       std::string text) {
    chrome_browser_sharing::SharingMessage message =
        SharedClipboardTestBase::CreateMessage(guid, device_name);
    message.mutable_remote_copy_message()->set_text(text);
    return message;
  }

 protected:
  std::unique_ptr<RemoteCopyMessageHandler> message_handler_;

  DISALLOW_COPY_AND_ASSIGN(RemoteCopyMessageHandlerTest);
};

}  // namespace

TEST_F(RemoteCopyMessageHandlerTest, NotificationWithoutDeviceName) {
  message_handler_->OnMessage(
      CreateMessage(base::GenerateGUID(), kEmptyDeviceName, kText),
      base::DoNothing());
  EXPECT_EQ(GetClipboardText(), kText);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(
          IDS_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_NOTIFICATION_TITLE_UNKNOWN_DEVICE),
      GetNotification().title());
}

TEST_F(RemoteCopyMessageHandlerTest, NotificationWithDeviceName) {
  message_handler_->OnMessage(
      CreateMessage(base::GenerateGUID(), kDeviceNameInMessage, kText),
      base::DoNothing());
  EXPECT_EQ(GetClipboardText(), kText);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_NOTIFICATION_TITLE,
                base::ASCIIToUTF16(kDeviceNameInMessage)),
            GetNotification().title());
}
