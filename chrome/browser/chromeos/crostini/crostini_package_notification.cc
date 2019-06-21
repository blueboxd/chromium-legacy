// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_package_notification.h"

#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "chrome/browser/chromeos/crostini/crostini_package_service.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace crostini {

namespace {

constexpr char kNotifierCrostiniPackageOperation[] =
    "crostini.package_operation";

}  // namespace

CrostiniPackageNotification::NotificationSettings::NotificationSettings() {}
CrostiniPackageNotification::NotificationSettings::NotificationSettings(
    const NotificationSettings& rhs) = default;
CrostiniPackageNotification::NotificationSettings::~NotificationSettings() {}

CrostiniPackageNotification::CrostiniPackageNotification(
    Profile* profile,
    NotificationType notification_type,
    PackageOperationStatus status,
    const base::string16& app_name,
    const std::string& notification_id,
    CrostiniPackageService* package_service)
    : notification_type_(notification_type),
      current_status_(status),
      package_service_(package_service),
      profile_(profile),
      notification_settings_(
          GetNotificationSettingsForTypeAndAppName(notification_type,
                                                   app_name)),
      visible_(true),
      weak_ptr_factory_(this) {
  if (status == PackageOperationStatus::RUNNING) {
    running_start_time_ = base::TimeTicks::Now();
  }
  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.vector_small_image = &ash::kNotificationLinuxIcon;
  rich_notification_data.never_timeout = true;
  rich_notification_data.accent_color = ash::kSystemNotificationColorNormal;

  notification_ = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_PROGRESS, notification_id,
      base::string16(), base::string16(),
      gfx::Image(),  // icon
      notification_settings_.source,
      GURL(),  // origin_url
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierCrostiniPackageOperation),
      rich_notification_data,
      base::MakeRefCounted<message_center::ThunkNotificationDelegate>(
          weak_ptr_factory_.GetWeakPtr()));

  // Sets title and body
  UpdateProgress(status, 0 /*progress_percent*/);
}

CrostiniPackageNotification::~CrostiniPackageNotification() = default;

PackageOperationStatus CrostiniPackageNotification::GetOperationStatus() const {
  return current_status_;
}

// static
CrostiniPackageNotification::NotificationSettings
CrostiniPackageNotification::GetNotificationSettingsForTypeAndAppName(
    NotificationType notification_type,
    const base::string16& app_name) {
  NotificationSettings result;

  switch (notification_type) {
    case NotificationType::PACKAGE_INSTALL:
      DCHECK(app_name.empty());
      result.source = l10n_util::GetStringUTF16(
          IDS_CROSTINI_PACKAGE_INSTALL_NOTIFICATION_DISPLAY_SOURCE);
      result.progress_title = l10n_util::GetStringUTF16(
          IDS_CROSTINI_PACKAGE_INSTALL_NOTIFICATION_IN_PROGRESS_TITLE);
      result.progress_body.clear();
      result.success_title = l10n_util::GetStringUTF16(
          IDS_CROSTINI_PACKAGE_INSTALL_NOTIFICATION_COMPLETED_TITLE);
      result.success_body = l10n_util::GetStringUTF16(
          IDS_CROSTINI_PACKAGE_INSTALL_NOTIFICATION_COMPLETED_MESSAGE);
      result.failure_title = l10n_util::GetStringUTF16(
          IDS_CROSTINI_PACKAGE_INSTALL_NOTIFICATION_ERROR_TITLE);
      result.failure_body = l10n_util::GetStringUTF16(
          IDS_CROSTINI_PACKAGE_INSTALL_NOTIFICATION_ERROR_MESSAGE);
      break;

    case NotificationType::APPLICATION_UNINSTALL:
      result.source = l10n_util::GetStringUTF16(
          IDS_CROSTINI_APPLICATION_UNINSTALL_NOTIFICATION_DISPLAY_SOURCE);
      result.queued_title = l10n_util::GetStringFUTF16(
          IDS_CROSTINI_APPLICATION_UNINSTALL_NOTIFICATION_QUEUED_TITLE,
          app_name);
      result.queued_body = l10n_util::GetStringUTF16(
          IDS_CROSTINI_APPLICATION_UNINSTALL_NOTIFICATION_QUEUED_MESSAGE);
      result.progress_title = l10n_util::GetStringFUTF16(
          IDS_CROSTINI_APPLICATION_UNINSTALL_NOTIFICATION_IN_PROGRESS_TITLE,
          app_name);
      result.success_title = l10n_util::GetStringFUTF16(
          IDS_CROSTINI_APPLICATION_UNINSTALL_NOTIFICATION_COMPLETED_TITLE,
          app_name);
      result.success_body = l10n_util::GetStringUTF16(
          IDS_CROSTINI_APPLICATION_UNINSTALL_NOTIFICATION_COMPLETED_MESSAGE);
      result.failure_title = l10n_util::GetStringFUTF16(
          IDS_CROSTINI_APPLICATION_UNINSTALL_NOTIFICATION_ERROR_TITLE,
          app_name);
      result.failure_body = l10n_util::GetStringUTF16(
          IDS_CROSTINI_APPLICATION_UNINSTALL_NOTIFICATION_ERROR_MESSAGE);
      break;

    default:
      NOTREACHED();
  }

  return result;
}

// TODO(timloh): This doesn't get called if the user shuts down Crostini, so
// the notification will be stuck at whatever percentage it is at.
void CrostiniPackageNotification::UpdateProgress(PackageOperationStatus status,
                                                 int progress_percent) {
  if (status == PackageOperationStatus::RUNNING &&
      current_status_ != PackageOperationStatus::RUNNING) {
    running_start_time_ = base::TimeTicks::Now();
  }
  current_status_ = status;

  base::string16 title;
  base::string16 body;
  message_center::NotificationType notification_type =
      message_center::NOTIFICATION_TYPE_SIMPLE;
  bool never_timeout = false;

  switch (status) {
    case PackageOperationStatus::SUCCEEDED:
      title = notification_settings_.success_title;
      body = notification_settings_.success_body;
      break;

    case PackageOperationStatus::FAILED:
      title = notification_settings_.failure_title;
      body = notification_settings_.failure_body;
      notification_->set_accent_color(
          ash::kSystemNotificationColorCriticalWarning);
      break;

    case PackageOperationStatus::WAITING_FOR_APP_REGISTRY_UPDATE:
      // If a notification progress bar is set to a value outside of [0, 100],
      // it becomes in infinite progress bar. Do that here because we have no
      // way to know how long this will take or how close we are to completion.
      progress_percent = -1;
      FALLTHROUGH;
    case PackageOperationStatus::RUNNING:
      never_timeout = true;
      notification_type = message_center::NOTIFICATION_TYPE_PROGRESS;
      title = notification_settings_.progress_title;
      if (notification_type_ == NotificationType::APPLICATION_UNINSTALL &&
          progress_percent >= 0) {
        // Uninstalls have a time remaining instead of a fixed message.
        body = GetTimeRemainingMessage(running_start_time_, progress_percent);

        // else leave body blank
      } else {
        body = notification_settings_.progress_body;
      }
      break;

    case PackageOperationStatus::QUEUED:
      // We don't have queued strings for some NotificationTypes; we shouldn't
      // be asked to move to QUEUED status for those,
      DCHECK(!notification_settings_.queued_title.empty());
      DCHECK(!notification_settings_.queued_body.empty());
      title = notification_settings_.queued_title;
      body = notification_settings_.queued_body;
      break;

    default:
      NOTREACHED();
  }

  notification_->set_title(title);
  notification_->set_message(body);
  notification_->set_type(notification_type);
  notification_->set_progress(progress_percent);
  notification_->set_never_timeout(never_timeout);
  UpdateDisplayedNotification();
}

void CrostiniPackageNotification::ForceAllowAutoHide() {
  notification_->set_never_timeout(false);
  UpdateDisplayedNotification();
}

void CrostiniPackageNotification::Close(bool by_user) {
  if (current_status_ == PackageOperationStatus::RUNNING ||
      current_status_ == PackageOperationStatus::QUEUED) {
    // We don't want to delete ourselves yet; we want to forcibly redisplay
    // when we hit success or failure. Just note that we are hidden.
    visible_ = false;
  } else {
    // This call deletes us.
    package_service_->NotificationCompleted(this);
  }
}

void CrostiniPackageNotification::UpdateDisplayedNotification() {
  if (current_status_ == PackageOperationStatus::SUCCEEDED ||
      current_status_ == PackageOperationStatus::FAILED) {
    // If the user closes the notification when it is queued or running, we
    // still want to tell them when it is actually finished. So force the
    // notification back to visibility when we get our success / fail notice.
    // Note that we only get one success / fail notice, so we won't keep
    // reshowing this.
    visible_ = true;
  }

  if (!visible_) {
    // User hid, don't re-display.
    return;
  }

  NotificationDisplayService* display_service =
      NotificationDisplayService::GetForProfile(profile_);
  display_service->Display(NotificationHandler::Type::TRANSIENT, *notification_,
                           /*metadata=*/nullptr);
}

}  // namespace crostini
