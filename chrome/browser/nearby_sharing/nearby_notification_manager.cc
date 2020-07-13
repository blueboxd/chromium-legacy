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

int GetFileAttachmentsStringId(const std::vector<FileAttachment>& files) {
  switch (GetCommonFileAttachmentType(files)) {
    case FileAttachment::Type::kApp:
      return IDS_NEARBY_FILE_ATTACHMENTS_APPS;
    case FileAttachment::Type::kImage:
      return IDS_NEARBY_FILE_ATTACHMENTS_IMAGES;
    case FileAttachment::Type::kUnknown:
      return IDS_NEARBY_FILE_ATTACHMENTS_UNKNOWN;
    case FileAttachment::Type::kVideo:
      return IDS_NEARBY_FILE_ATTACHMENTS_VIDEOS;
    default:
      return IDS_NEARBY_UNKNOWN_ATTACHMENTS;
  }
}

int GetTextAttachmentsStringId(const std::vector<TextAttachment>& texts) {
  switch (GetCommonTextAttachmentType(texts)) {
    case TextAttachment::Type::kAddress:
      return IDS_NEARBY_TEXT_ATTACHMENTS_ADDRESSES;
    case TextAttachment::Type::kPhoneNumber:
      return IDS_NEARBY_TEXT_ATTACHMENTS_PHONE_NUMBERS;
    case TextAttachment::Type::kText:
      return IDS_NEARBY_TEXT_ATTACHMENTS_UNKNOWN;
    case TextAttachment::Type::kUrl:
      return IDS_NEARBY_TEXT_ATTACHMENTS_LINKS;
    default:
      return IDS_NEARBY_UNKNOWN_ATTACHMENTS;
  }
}

base::string16 GetAttachmentsString(const ShareTarget& share_target) {
  size_t file_count = share_target.file_attachments().size();
  size_t text_count = share_target.text_attachments().size();
  int resource_id = IDS_NEARBY_UNKNOWN_ATTACHMENTS;

  if (file_count > 0 && text_count == 0)
    resource_id = GetFileAttachmentsStringId(share_target.file_attachments());

  if (text_count > 0 && file_count == 0)
    resource_id = GetTextAttachmentsStringId(share_target.text_attachments());

  return l10n_util::GetPluralStringFUTF16(resource_id, text_count + file_count);
}

base::string16 GetNotificationTitle(const ShareTarget& share_target) {
  int resource_id = share_target.is_incoming()
                        ? IDS_NEARBY_NOTIFICATION_RECEIVE_PROGRESS_TITLE
                        : IDS_NEARBY_NOTIFICATION_SEND_PROGRESS_TITLE;
  base::string16 attachments = GetAttachmentsString(share_target);
  base::string16 device_name = base::ASCIIToUTF16(share_target.device_name());

  return l10n_util::GetStringFUTF16(resource_id, attachments, device_name);
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
      GetNotificationTitle(share_target),
      /*message=*/base::string16(),
      /*icon=*/gfx::Image(),
      l10n_util::GetStringUTF16(IDS_NEARBY_NOTIFICATION_SOURCE),
      /*origin_url=*/GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNearbyNotifier),
      rich_notification_data,
      /*delegate=*/nullptr);

  notification.set_vector_small_image(kNearbyShareIcon);
  notification.set_progress(100.0 * transfer_metadata.progress());

  std::vector<message_center::ButtonInfo> notification_actions;
  notification_actions.emplace_back(l10n_util::GetStringUTF16(IDS_APP_CANCEL));
  notification.set_buttons(notification_actions);

  NotificationDisplayServiceFactory::GetForProfile(profile_)->Display(
      NotificationHandler::Type::NEARBY_SHARE, notification,
      /*metadata=*/nullptr);
}
