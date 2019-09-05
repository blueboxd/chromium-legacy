// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_message_handler_desktop.h"

#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/grit/generated_resources.h"
#include "components/sync_device_info/device_info.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "url/gurl.h"

SharedClipboardMessageHandlerDesktop::SharedClipboardMessageHandlerDesktop(
    SharingService* sharing_service,
    NotificationDisplayService* notification_display_service)
    : SharedClipboardMessageHandler(sharing_service),
      notification_display_service_(notification_display_service) {}

SharedClipboardMessageHandlerDesktop::~SharedClipboardMessageHandlerDesktop() =
    default;

void SharedClipboardMessageHandlerDesktop::ShowNotification(
    std::unique_ptr<syncer::DeviceInfo> device_info) {
  DCHECK(notification_display_service_);
  DCHECK(device_info);

  std::string notification_id = base::GenerateGUID();
  std::string device_name = device_info->client_name();

  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      l10n_util::GetStringFUTF16(
          IDS_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_NOTIFICATION_TITLE,
          base::UTF8ToUTF16(device_name)),
      l10n_util::GetStringUTF16(
          IDS_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_NOTIFICATION_DESCRIPTION),
      /* icon= */ gfx::Image(),
      /* display_source= */ base::string16(),
      /* origin_url= */ GURL(), message_center::NotifierId(),
      message_center::RichNotificationData(),
      /* delegate= */ nullptr);

  notification_display_service_->Display(NotificationHandler::Type::SHARING,
                                         notification, /* metadata= */ nullptr);
}