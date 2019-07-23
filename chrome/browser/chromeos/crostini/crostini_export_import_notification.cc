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
#include "ui/base/text/bytes_formatting.h"
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
    ExportImportType type,
    const std::string& notification_id,
    const base::FilePath& path)
    : profile_(profile),
      type_(type),
      path_(path),
      weak_ptr_factory_(this) {
  DCHECK(type == ExportImportType::EXPORT || type == ExportImportType::IMPORT);

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

  SetStatusRunning(0);
}

CrostiniExportImportNotification::~CrostiniExportImportNotification() = default;

void CrostiniExportImportNotification::SetStatusRunning(int progress_percent) {
  status_ = Status::RUNNING;

  if (closed_) {
    return;
  }

  notification_->set_type(message_center::NOTIFICATION_TYPE_PROGRESS);
  notification_->set_accent_color(ash::kSystemNotificationColorNormal);
  notification_->set_title(l10n_util::GetStringUTF16(
      type_ == ExportImportType::EXPORT
          ? IDS_CROSTINI_EXPORT_NOTIFICATION_TITLE_RUNNING
          : IDS_CROSTINI_IMPORT_NOTIFICATION_TITLE_RUNNING));
  notification_->set_message(
      GetTimeRemainingMessage(started_, progress_percent));
  notification_->set_buttons({});
  notification_->set_never_timeout(true);
  notification_->set_progress(progress_percent);

  NotificationDisplayService::GetForProfile(profile_)->Display(
      NotificationHandler::Type::TRANSIENT, *notification_,
      /*metadata=*/nullptr);
}

void CrostiniExportImportNotification::SetStatusDone() {
  status_ = Status::DONE;

  notification_->set_type(message_center::NOTIFICATION_TYPE_SIMPLE);
  notification_->set_accent_color(ash::kSystemNotificationColorNormal);
  notification_->set_title(l10n_util::GetStringUTF16(
      type_ == ExportImportType::EXPORT
          ? IDS_CROSTINI_EXPORT_NOTIFICATION_TITLE_DONE
          : IDS_CROSTINI_IMPORT_NOTIFICATION_TITLE_DONE));
  notification_->set_message(l10n_util::GetStringUTF16(
      type_ == ExportImportType::EXPORT
          ? IDS_CROSTINI_EXPORT_NOTIFICATION_MESSAGE_DONE
          : IDS_CROSTINI_IMPORT_NOTIFICATION_MESSAGE_DONE));
  notification_->set_buttons(
      type_ == ExportImportType::EXPORT
          ? std::vector<message_center::ButtonInfo>{message_center::ButtonInfo(
                l10n_util::GetStringUTF16(IDS_DOWNLOAD_LINK_SHOW))}
          : std::vector<message_center::ButtonInfo>{});
  notification_->set_never_timeout(false);

  NotificationDisplayService::GetForProfile(profile_)->Display(
      NotificationHandler::Type::TRANSIENT, *notification_,
      /*metadata=*/nullptr);
}

void CrostiniExportImportNotification::SetStatusFailed() {
  SetStatusFailed(l10n_util::GetStringUTF16(
      type_ == ExportImportType::EXPORT
          ? IDS_CROSTINI_EXPORT_NOTIFICATION_MESSAGE_FAILED
          : IDS_CROSTINI_IMPORT_NOTIFICATION_MESSAGE_FAILED));
}

void CrostiniExportImportNotification::SetStatusFailedArchitectureMismatch(
    const std::string& architecture_container,
    const std::string& architecture_device) {
  DCHECK(type_ == ExportImportType::IMPORT);
  SetStatusFailed(l10n_util::GetStringFUTF16(
      IDS_CROSTINI_IMPORT_NOTIFICATION_MESSAGE_FAILED_ARCHITECTURE,
      base::ASCIIToUTF16(architecture_container),
      base::ASCIIToUTF16(architecture_device)));
}

void CrostiniExportImportNotification::SetStatusFailedInsufficientSpace(
    uint64_t additional_required_space) {
  DCHECK(type_ == ExportImportType::IMPORT);
  SetStatusFailed(l10n_util::GetStringFUTF16(
      IDS_CROSTINI_IMPORT_NOTIFICATION_MESSAGE_FAILED_SPACE,
      ui::FormatBytes(additional_required_space)));
}

void CrostiniExportImportNotification::SetStatusFailedConcurrentOperation(
    ExportImportType in_progress_operation_type) {
  SetStatusFailed(l10n_util::GetStringUTF16(
      in_progress_operation_type == ExportImportType::EXPORT
          ? IDS_CROSTINI_EXPORT_NOTIFICATION_MESSAGE_FAILED_IN_PROGRESS
          : IDS_CROSTINI_IMPORT_NOTIFICATION_MESSAGE_FAILED_IN_PROGRESS));
}

void CrostiniExportImportNotification::SetStatusFailed(
    const base::string16& message) {
  status_ = Status::FAILED;

  notification_->set_type(message_center::NOTIFICATION_TYPE_SIMPLE);
  notification_->set_accent_color(ash::kSystemNotificationColorCriticalWarning);
  notification_->set_title(l10n_util::GetStringUTF16(
      type_ == ExportImportType::EXPORT
          ? IDS_CROSTINI_EXPORT_NOTIFICATION_TITLE_FAILED
          : IDS_CROSTINI_IMPORT_NOTIFICATION_TITLE_FAILED));
  notification_->set_message(message);
  notification_->set_buttons({});
  notification_->set_never_timeout(false);

  NotificationDisplayService::GetForProfile(profile_)->Display(
      NotificationHandler::Type::TRANSIENT, *notification_,
      /*metadata=*/nullptr);
}

void CrostiniExportImportNotification::Close(bool by_user) {
  closed_ = true;
  if (status_ != Status::RUNNING) {
    delete this;
  }
}

void CrostiniExportImportNotification::Click(
    const base::Optional<int>& button_index,
    const base::Optional<base::string16>&) {
  if (!button_index) {
    return;
  }

  switch (status_) {
    case Status::DONE:
      if (type_ == ExportImportType::EXPORT) {
        DCHECK(*button_index == 1);
        platform_util::ShowItemInFolder(profile_, path_);
      } else {
        NOTREACHED();
      }
      return;
    default:
      NOTREACHED();
  }
}

}  // namespace crostini
