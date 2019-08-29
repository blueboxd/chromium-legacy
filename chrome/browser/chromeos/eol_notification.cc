// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/eol_notification.h"

#include "ash/public/cpp/notification_utils.h"
#include "base/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/default_clock.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/update_engine_client.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/public/cpp/notification.h"

using l10n_util::GetStringUTF16;

namespace chromeos {
namespace {

const char kEolNotificationId[] = "chrome://product_eol";

base::string16 PredictedMonthAndYearOfEol(base::Time current_time,
                                          int32_t number_of_milestones) {
  // The average number of weeks between automatic updates to the OS.  There is
  // an 8 week cycle every 6 months, so the worst case earliest predicted date
  // can be 4 weeks/year behind the actual final release date.  Since the date
  // of the first release milestone out of the |number_of_milestones| remaining
  // is not provided, the worst case latest prediction could be up to but not
  // including 6 weeks ahead of the actual final release date.  Underestimation
  // is preferred, so 6 week intervals are used.
  constexpr int kAverageNumWeeksInMilestone = 6;
  constexpr int kNumDaysInOneWeek = 7;

  base::Time predicted_eol_date =
      current_time +
      base::TimeDelta::FromDays(kAverageNumWeeksInMilestone *
                                kNumDaysInOneWeek * number_of_milestones);
  return base::TimeFormatMonthAndYear(predicted_eol_date);
}

// Buttons that appear in notifications.
enum ButtonIndex {
  BUTTON_MORE_INFO = 0,
  BUTTON_DISMISS,
  BUTTON_SIZE = BUTTON_DISMISS
};

class EolNotificationDelegate : public message_center::NotificationDelegate {
 public:
  explicit EolNotificationDelegate(Profile* profile) : profile_(profile) {}

 private:
  ~EolNotificationDelegate() override = default;

  // NotificationDelegate overrides:
  void Click(const base::Optional<int>& button_index,
             const base::Optional<base::string16>& reply) override {
    if (!button_index)
      return;

    switch (*button_index) {
      case BUTTON_MORE_INFO: {
        // show eol link
        NavigateParams params(profile_, GURL(chrome::kEolNotificationURL),
                              ui::PAGE_TRANSITION_LINK);
        params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
        params.window_action = NavigateParams::SHOW_WINDOW;
        Navigate(&params);
        break;
      }
      case BUTTON_DISMISS:
        // set dismiss pref.
        profile_->GetPrefs()->SetBoolean(prefs::kEolNotificationDismissed,
                                         true);
        break;
    }
    NotificationDisplayServiceFactory::GetForProfile(profile_)->Close(
        NotificationHandler::Type::TRANSIENT, kEolNotificationId);
  }

  Profile* const profile_;

  DISALLOW_COPY_AND_ASSIGN(EolNotificationDelegate);
};

}  // namespace

// static
bool EolNotification::ShouldShowEolNotification() {
  // Do not show end of life notification if this device is managed by
  // enterprise user.
  if (g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->IsEnterpriseManaged()) {
    return false;
  }

  return true;
}

EolNotification::EolNotification(Profile* profile)
    : clock_(base::DefaultClock::GetInstance()),
      profile_(profile),
      status_(update_engine::EndOfLifeStatus::kSupported) {}

EolNotification::~EolNotification() {}

void EolNotification::CheckEolStatus() {
  UpdateEngineClient* update_engine_client =
      DBusThreadManager::Get()->GetUpdateEngineClient();

  // Request the Eol Status.
  update_engine_client->GetEolStatus(base::BindOnce(
      &EolNotification::OnEolStatus, weak_factory_.GetWeakPtr()));
}

void EolNotification::OnEolStatus(
    update_engine::EndOfLifeStatus status,
    base::Optional<int32_t> number_of_milestones) {
  status_ = status;
  number_of_milestones_ = std::move(number_of_milestones);

  const int pre_eol_status =
      profile_->GetPrefs()->GetInteger(prefs::kEolStatus);
  profile_->GetPrefs()->SetInteger(prefs::kEolStatus, status_);

  // Security only state is no longer supported.  If |number_of_milestones_| is
  // non-empty, a notification should appear regardless of |status_| alone.
  if (!number_of_milestones_ &&
      (status_ == update_engine::EndOfLifeStatus::kSupported ||
       status_ == update_engine::EndOfLifeStatus::kSecurityOnly)) {
    return;
  }

  if (pre_eol_status != status_) {
    // If Eol status has changed, we should reset
    // kEolNotificationDismissed and show notification.
    profile_->GetPrefs()->SetBoolean(prefs::kEolNotificationDismissed, false);
  }

  bool user_dismissed_eol_notification =
      profile_->GetPrefs()->GetBoolean(prefs::kEolNotificationDismissed);
  if (user_dismissed_eol_notification)
    return;

  Update();
}

void EolNotification::Update() {
  message_center::RichNotificationData data;
  std::unique_ptr<message_center::Notification> notification;

  DCHECK_EQ(BUTTON_MORE_INFO, data.buttons.size());
  data.buttons.emplace_back(GetStringUTF16(IDS_LEARN_MORE));

  if (number_of_milestones_ && number_of_milestones_.value() > 0) {
    // Notifies user that updates will stop occurring at a month and year.
    notification = ash::CreateSystemNotification(
        message_center::NOTIFICATION_TYPE_SIMPLE, kEolNotificationId,
        l10n_util::GetStringFUTF16(
            IDS_PENDING_EOL_NOTIFICATION_TITLE,
            PredictedMonthAndYearOfEol(clock_->Now(),
                                       number_of_milestones_.value())),
        l10n_util::GetStringFUTF16(IDS_PENDING_EOL_NOTIFICATION_MESSAGE,
                                   ui::GetChromeOSDeviceName()),
        base::string16() /* display_source */, GURL(kEolNotificationId),
        message_center::NotifierId(
            message_center::NotifierType::SYSTEM_COMPONENT, kEolNotificationId),
        data, new EolNotificationDelegate(profile_),
        vector_icons::kBusinessIcon,
        message_center::SystemNotificationWarningLevel::NORMAL);
  } else {
    // Notifies user that updates will no longer occur after this final update.
    DCHECK_EQ(BUTTON_DISMISS, data.buttons.size());
    data.buttons.emplace_back(GetStringUTF16(IDS_EOL_DISMISS_BUTTON));
    notification = ash::CreateSystemNotification(
        message_center::NOTIFICATION_TYPE_SIMPLE, kEolNotificationId,
        GetStringUTF16(IDS_EOL_NOTIFICATION_TITLE),
        l10n_util::GetStringFUTF16(IDS_EOL_NOTIFICATION_EOL,
                                   ui::GetChromeOSDeviceName()),
        base::string16() /* display_source */, GURL(kEolNotificationId),
        message_center::NotifierId(
            message_center::NotifierType::SYSTEM_COMPONENT, kEolNotificationId),
        data, new EolNotificationDelegate(profile_),
        kNotificationEndOfSupportIcon,
        message_center::SystemNotificationWarningLevel::NORMAL);
  }

  NotificationDisplayServiceFactory::GetForProfile(profile_)->Display(
      NotificationHandler::Type::TRANSIENT, *notification,
      /*metadata=*/nullptr);
}

}  // namespace chromeos
