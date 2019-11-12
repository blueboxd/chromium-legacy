// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/shared_clipboard/remote_copy_message_handler.h"

#include <memory>

#include "base/guid.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/sync/protocol/sharing_message.pb.h"
#include "components/sync/protocol/sharing_remote_copy_message.pb.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "url/gurl.h"

RemoteCopyMessageHandler::RemoteCopyMessageHandler(Profile* profile)
    : profile_(profile) {}

RemoteCopyMessageHandler::~RemoteCopyMessageHandler() = default;

void RemoteCopyMessageHandler::OnMessage(
    chrome_browser_sharing::SharingMessage message,
    DoneCallback done_callback) {
  DCHECK(message.has_remote_copy_message());

  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
      .WriteText(base::UTF8ToUTF16(message.remote_copy_message().text()));

  ShowNotification(message.sender_device_name());

  std::move(done_callback).Run(/*response=*/nullptr);
}

void RemoteCopyMessageHandler::ShowNotification(
    const std::string& device_name) {
  std::string notification_id = base::GenerateGUID();

  // TODO(mvanouwerkerk): Adjust notification text and icon once we have mocks.
  base::string16 notification_title =
      device_name.empty()
          ? l10n_util::GetStringUTF16(
                IDS_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_NOTIFICATION_TITLE_UNKNOWN_DEVICE)
          : l10n_util::GetStringFUTF16(
                IDS_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_NOTIFICATION_TITLE,
                base::UTF8ToUTF16(device_name));

  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      notification_title,
      l10n_util::GetStringUTF16(
          IDS_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_NOTIFICATION_DESCRIPTION),
      /*icon=*/gfx::Image(),
      /*display_source=*/base::string16(),
      /*origin_url=*/GURL(), message_center::NotifierId(),
      message_center::RichNotificationData(),
      /*delegate=*/nullptr);

  NotificationDisplayServiceFactory::GetForProfile(profile_)->Display(
      NotificationHandler::Type::SHARING, notification, /*metadata=*/nullptr);
}
