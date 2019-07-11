// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_export_import_notification.h"

#include "ash/public/cpp/notification_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/chromeos/crostini/crostini_export_import.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace crostini {

namespace {

constexpr char kNotifierCrostiniExportImportOperation[] =
    "crostini.export_operation";

}  // namespace

CrostiniExportImportNotification::CrostiniExportImportNotification(
    Profile* profile,
    CrostiniExportImport* service,
    ExportImportType type,
    const std::string& notification_id,
    const base::FilePath& path)
    : profile_(profile),
      service_(service),
      type_(type),
      path_(path),
      weak_ptr_factory_(this) {
  // Messages.
  switch (type) {
    case ExportImportType::EXPORT:
      title_running_ = l10n_util::GetStringUTF16(
          IDS_CROSTINI_EXPORT_NOTIFICATION_TITLE_RUNNING);
      title_done_ = l10n_util::GetStringUTF16(
          IDS_CROSTINI_EXPORT_NOTIFICATION_TITLE_DONE);
      message_done_ = l10n_util::GetStringUTF16(
          IDS_CROSTINI_EXPORT_NOTIFICATION_MESSAGE_DONE);
      title_failed_ = l10n_util::GetStringUTF16(
          IDS_CROSTINI_EXPORT_NOTIFICATION_TITLE_FAILED);
      message_failed_ = l10n_util::GetStringUTF16(
          IDS_CROSTINI_EXPORT_NOTIFICATION_MESSAGE_FAILED);
      break;
    case ExportImportType::IMPORT:
      title_running_ = l10n_util::GetStringUTF16(
          IDS_CROSTINI_IMPORT_NOTIFICATION_TITLE_RUNNING);
      title_done_ = l10n_util::GetStringUTF16(
          IDS_CROSTINI_IMPORT_NOTIFICATION_TITLE_DONE);
      message_done_ = l10n_util::GetStringUTF16(
          IDS_CROSTINI_IMPORT_NOTIFICATION_MESSAGE_DONE);
      title_failed_ = l10n_util::GetStringUTF16(
          IDS_CROSTINI_IMPORT_NOTIFICATION_TITLE_FAILED);
      message_failed_ = l10n_util::GetStringUTF16(
          IDS_CROSTINI_IMPORT_NOTIFICATION_MESSAGE_FAILED);
      break;
    default:
      NOTREACHED();
  }

  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.vector_small_image = &kNotificationLinuxIcon;
  rich_notification_data.accent_color = ash::kSystemNotificationColorNormal;

  notification_ = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_PROGRESS, notification_id,
      base::string16(), base::string16(),
      gfx::Image(),  // icon
      l10n_util::GetStringUTF16(
          IDS_CROSTINI_EXPORT_IMPORT_NOTIFICATION_DISPLAY_SOURCE),
      GURL(),  // origin_url
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierCrostiniExportImportOperation),
      rich_notification_data,
      base::MakeRefCounted<message_center::ThunkNotificationDelegate>(
          weak_ptr_factory_.GetWeakPtr()));

  UpdateStatus(Status::RUNNING, 0);
}

CrostiniExportImportNotification::~CrostiniExportImportNotification() = default;

void CrostiniExportImportNotification::UpdateStatus(Status status,
                                                    int progress_percent) {
  status_ = status;
  switch (status) {
    case Status::RUNNING:
      if (closed_) {
        return;
      }
      notification_->set_type(message_center::NOTIFICATION_TYPE_PROGRESS);
      notification_->set_progress(progress_percent);
      notification_->set_title(title_running_);
      notification_->set_message(
          GetTimeRemainingMessage(started_, progress_percent));
      notification_->set_never_timeout(true);
      break;
    case Status::DONE:
      notification_->set_type(message_center::NOTIFICATION_TYPE_SIMPLE);
      if (type_ == ExportImportType::EXPORT) {
        notification_->set_buttons({message_center::ButtonInfo(
            l10n_util::GetStringUTF16(IDS_DOWNLOAD_LINK_SHOW))});
      }
      notification_->set_title(title_done_);
      notification_->set_message(message_done_);
      notification_->set_never_timeout(false);
      break;
    case Status::FAILED:
      notification_->set_type(message_center::NOTIFICATION_TYPE_SIMPLE);
      notification_->set_accent_color(
          ash::kSystemNotificationColorCriticalWarning);
      notification_->set_title(title_failed_);
      notification_->set_message(message_failed_);
      notification_->set_never_timeout(false);
      break;
    default:
      NOTREACHED();
  }
  NotificationDisplayService::GetForProfile(profile_)->Display(
      NotificationHandler::Type::TRANSIENT, *notification_,
      /*metadata=*/nullptr);
}

void CrostiniExportImportNotification::Close(bool by_user) {
  closed_ = true;
  if (status_ != Status::RUNNING) {
    service_->NotificationCompleted(this);
  }
}

void CrostiniExportImportNotification::Click(
    const base::Optional<int>& button_index,
    const base::Optional<base::string16>& reply) {
  if (type_ == ExportImportType::EXPORT) {
    platform_util::ShowItemInFolder(profile_, path_);
  }
}

}  // namespace crostini
