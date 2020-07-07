// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_notification_manager.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/strings/grit/ui_strings.h"

namespace {

constexpr char kNearbyNotificationId[] = "chrome://nearby";
constexpr char kNearbyNotifier[] = "nearby";

FileAttachment::Type GetCommonFileAttachmentType(
    const std::vector<FileAttachment>& files) {
  if (files.empty())
    return FileAttachment::Type::kUnknown;

  FileAttachment::Type type = files[0].type();
  for (size_t i = 1; i < files.size(); ++i) {
    if (files[i].type() != type)
      return FileAttachment::Type::kUnknown;
  }
  return type;
}

TextAttachment::Type GetCommonTextAttachmentType(
    const std::vector<TextAttachment>& texts) {
  if (texts.empty())
    return TextAttachment::Type::kText;

  TextAttachment::Type type = texts[0].type();
  for (size_t i = 1; i < texts.size(); ++i) {
    if (texts[i].type() != type)
      return TextAttachment::Type::kText;
  }
  return type;
}

base::string16 GetUnknownAttachmentsString(size_t count) {
  // TODO(crbug.com/1102348): Provide translated string.
  return base::string16();
}

base::string16 GetFileAttachmentsString(
    const std::vector<FileAttachment>& files) {
  // TODO(crbug.com/1102348): Add translated special cases for file types.
  switch (GetCommonFileAttachmentType(files)) {
    default:
      return GetUnknownAttachmentsString(files.size());
  }
}

base::string16 GetTextAttachmentsString(
    const std::vector<TextAttachment>& texts) {
  // TODO(crbug.com/1102348): Add translated special cases for text types.
  switch (GetCommonTextAttachmentType(texts)) {
    default:
      return GetUnknownAttachmentsString(texts.size());
  }
}

base::string16 GetAttachmentsString(const ShareTarget& share_target) {
  size_t file_count = share_target.file_attachments().size();
  size_t text_count = share_target.text_attachments().size();

  if (file_count > 0 && text_count == 0)
    return GetFileAttachmentsString(share_target.file_attachments());

  if (text_count > 0 && file_count == 0)
    return GetTextAttachmentsString(share_target.text_attachments());

  return GetUnknownAttachmentsString(file_count + text_count);
}

}  // namespace

NearbyNotificationManager::NearbyNotificationManager(Profile* profile)
    : profile_(profile) {}

NearbyNotificationManager::~NearbyNotificationManager() = default;

void NearbyNotificationManager::ShowProgress(
    const ShareTarget& share_target,
    const TransferMetadata& transfer_metadata) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.never_timeout = true;

  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_PROGRESS, kNearbyNotificationId,
      // TODO(crbug.com/1102348): Provide translated title.
      /*title=*/GetAttachmentsString(share_target),
      /*message=*/base::string16(),
      /*icon=*/gfx::Image(),
      // TODO(crbug.com/1102348): Provide translated source.
      /*display_source=*/base::string16(),
      /*origin_url=*/GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNearbyNotifier),
      rich_notification_data,
      /*delegate=*/nullptr);

  // TODO(crbug.com/1102348): Set Nearby Share icon.
  notification.set_progress(100.0 * transfer_metadata.progress());

  std::vector<message_center::ButtonInfo> notification_actions;
  notification_actions.emplace_back(l10n_util::GetStringUTF16(IDS_APP_CANCEL));
  notification.set_buttons(notification_actions);

  NotificationDisplayServiceFactory::GetForProfile(profile_)->Display(
      NotificationHandler::Type::NEARBY_SHARE, notification,
      /*metadata=*/nullptr);
}
