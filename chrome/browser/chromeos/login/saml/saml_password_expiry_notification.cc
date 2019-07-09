// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/saml/saml_password_expiry_notification.h"

#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/session/session_activation_observer.h"
#include "ash/public/cpp/session/session_controller.h"
#include "base/bind.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/in_session_password_change/password_change_ui.h"
#include "chrome/common/pref_names.h"
#include "chromeos/login/auth/saml_password_attributes.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

using message_center::ButtonInfo;
using message_center::HandleNotificationClickDelegate;
using message_center::Notification;
using message_center::NotificationObserver;
using message_center::NotificationType;
using message_center::NotifierId;
using message_center::NotifierType;
using message_center::RichNotificationData;
using message_center::SystemNotificationWarningLevel;
using message_center::ThunkNotificationDelegate;

namespace chromeos {

namespace {

// Unique ID for this notification.
const char kNotificationId[] = "saml.password-expiry-notification";

// NotifierId for histogram reporting.
const base::NoDestructor<NotifierId> kNotifierId(
    message_center::NotifierType::SYSTEM_COMPONENT,
    kNotificationId);

// Simplest type of notification UI - no progress bars, images etc.
const NotificationType kNotificationType =
    message_center::NOTIFICATION_TYPE_SIMPLE;

// Generic type for notifications that are not from web pages etc.
const NotificationHandler::Type kNotificationHandlerType =
    NotificationHandler::Type::TRANSIENT;

// The icon to use for this notification - looks like an office building.
const gfx::VectorIcon& kIcon = vector_icons::kBusinessIcon;

// Leaving this empty means the notification is attributed to the system -
// ie "Chromium OS" or similar.
const base::NoDestructor<base::string16> kEmptyDisplaySource;

// No origin URL is needed since the notification comes from the system.
const base::NoDestructor<GURL> kEmptyOriginUrl;

// When the password will expire in |kCriticalWarningDays| or less, the warning
// will have red text, slightly stronger language, and doesn't automatically
// time out - it stays on the screen until the user hides or dismisses it.
const int kCriticalWarningDays = 3;

base::string16 GetTitleText(int less_than_n_days) {
  return l10n_util::GetPluralStringFUTF16(IDS_PASSWORD_EXPIRY_DAYS_TITLE,
                                          less_than_n_days);
}

base::string16 GetBodyText(bool is_critical) {
  return is_critical
             ? l10n_util::GetStringUTF16(
                   IDS_PASSWORD_EXPIRY_CALL_TO_ACTION_CRITICAL)
             : l10n_util::GetStringUTF16(IDS_PASSWORD_EXPIRY_CALL_TO_ACTION);
}

RichNotificationData GetRichNotificationData(bool is_critical) {
  RichNotificationData result;
  result.never_timeout = is_critical;
  result.buttons = std::vector<ButtonInfo>{ButtonInfo(
      l10n_util::GetStringUTF16(IDS_PASSWORD_EXPIRY_CHANGE_PASSWORD_BUTTON))};
  return result;
}

SystemNotificationWarningLevel GetWarningLevel(bool is_critical) {
  return is_critical ? SystemNotificationWarningLevel::CRITICAL_WARNING
                     : SystemNotificationWarningLevel::WARNING;
}

void ShowNotificationImpl(
    Profile* profile,
    int less_than_n_days,
    scoped_refptr<message_center::NotificationDelegate> delegate) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const bool is_critical = less_than_n_days <= kCriticalWarningDays;
  const base::string16 title = GetTitleText(less_than_n_days);
  const base::string16 body = GetBodyText(is_critical);
  const RichNotificationData rich_notification_data =
      GetRichNotificationData(is_critical);
  const SystemNotificationWarningLevel warning_level =
      GetWarningLevel(is_critical);

  std::unique_ptr<Notification> notification = ash::CreateSystemNotification(
      kNotificationType, kNotificationId, title, body, *kEmptyDisplaySource,
      *kEmptyOriginUrl, *kNotifierId, rich_notification_data, delegate, kIcon,
      warning_level);

  NotificationDisplayService* nds =
      NotificationDisplayServiceFactory::GetForProfile(profile);
  // Calling close before display ensures that the notification pops up again
  // even if it is already shown.
  nds->Close(kNotificationHandlerType, kNotificationId);
  nds->Display(kNotificationHandlerType, *notification, /*metadata=*/nullptr);
}

// A time delta of length one hour.
const base::TimeDelta kOneHour = base::TimeDelta::FromHours(1);
// A time delta of length one day.
const base::TimeDelta kOneDay = base::TimeDelta::FromDays(1);

// Traits for running RecheckTask. Runs from the UI thread to show notification.
const base::TaskTraits kRecheckTaskTraits = {
    content::BrowserThread::UI, base::TaskPriority::BEST_EFFORT,
    base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};

// Returns true if |profile| is still valid.
bool IsValidProfile(Profile* profile) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  return profile_manager && profile_manager->IsValidProfile(profile);
}

// Returns true if showing the notification is enabled for this profile.
bool IsEnabledForProfile(Profile* profile) {
  return chromeos::ProfileHelper::IsPrimaryProfile(profile) &&
         profile->GetPrefs()->GetBoolean(
             prefs::kSamlInSessionPasswordChangeEnabled);
}

// The Rechecker checks periodically if the notification should be shown or
// updated. When it checks depends on when the password will expire.
// There is only at most one Rechecker at a time - for the primary user.
class Rechecker : public ash::SessionActivationObserver,
                  public NotificationObserver {
 public:
  // Shows the notification for the primary profile.
  void ShowNotification(int less_than_n_days);

  // Checks again if the notification should be shown, and maybe shows it.
  void Recheck();

  // Calls recheck after the given |delay|.
  void RecheckAfter(base::TimeDelta delay);

  // Cancels any pending tasks to call Recheck again.
  void CancelPendingRecheck();

  // ash::SessionActivationObserver:
  void OnSessionActivated(bool activated) override {}  // Not needed.
  void OnLockStateChanged(bool locked) override;

  // NotificationObserver:
  void Click(const base::Optional<int>& button_index,
             const base::Optional<base::string16>& reply) override;
  void Close(bool by_user) override;

  // Ensures the singleton is initialized and started for the given profile.
  static void StartForProfile(Profile* profile);
  // Stops and deletes the Rechecker singleton.
  static void Stop();

 private:
  // The constructor and destructor also maintain the singleton instance.
  Rechecker(Profile* profile, const AccountId& account_id);
  ~Rechecker() override;

  // Singleton, since we only ever need one instance_ for the primary user.
  static Rechecker* instance_;

  Profile* profile_;
  const AccountId account_id_;
  bool reshow_on_unlock_ = false;

  // Weak ptr factory for handling interaction with notifications - these
  // pointers shouldn't be invalidated until this class is deleted.
  base::WeakPtrFactory<NotificationObserver> observer_weak_ptr_factory_{this};
  // Weak ptr factory for posting tasks. Invalidating these pointers will
  // cancel upcoming tasks.
  base::WeakPtrFactory<Rechecker> task_weak_ptr_factory_{this};

  // Give test helper access to internals.
  friend SamlPasswordExpiryNotificationTestHelper;

  DISALLOW_COPY_AND_ASSIGN(Rechecker);
};

void Rechecker::ShowNotification(int less_than_n_days) {
  ShowNotificationImpl(profile_, less_than_n_days,
                       base::MakeRefCounted<ThunkNotificationDelegate>(
                           observer_weak_ptr_factory_.GetWeakPtr()));
  // When a notification is currently showing, reshow it on every unlock.
  reshow_on_unlock_ = true;
}

void Rechecker::Recheck() {
  // This cancels any pending call to Recheck - we don't want some bug to cause
  // us to queue up lots of calls to Recheck in the future, we only want one.
  CancelPendingRecheck();

  // In case the profile has been deleted since this task was posted.
  if (!IsValidProfile(profile_)) {
    delete this;  // No need to keep calling recheck.
    return;
  }

  PrefService* prefs = profile_->GetPrefs();
  SamlPasswordAttributes attrs = SamlPasswordAttributes::LoadFromPrefs(prefs);
  if (!IsEnabledForProfile(profile_) || !attrs.has_expiration_time()) {
    // Feature is not enabled for this profile, or profile is not primary, or
    // there is no expiration time. Dismiss if shown, and stop checking.
    DismissSamlPasswordExpiryNotification(profile_);
    delete this;
    return;
  }

  // Calculate how many days until the password will expire.
  const base::TimeDelta time_until_expiry =
      attrs.expiration_time() - base::Time::Now();
  const int less_than_n_days =
      std::max(0, time_until_expiry.InDaysFloored() + 1);
  const int advance_warning_days = std::max(
      0, prefs->GetInteger(prefs::kSamlPasswordExpirationAdvanceWarningDays));

  if (less_than_n_days <= advance_warning_days) {
    // The password is expired, or expires in less than |advance_warning_days|.
    // So we show a notification immediately.
    ShowNotification(less_than_n_days);
    // We check again whether to reshow / update the notification after one day:
    RecheckAfter(kOneDay);

  } else {
    // We have not yet reached the advance warning threshold. Check again
    // once we have arrived at expiry_time minus advance_warning_days...
    base::TimeDelta recheck_delay =
        time_until_expiry - base::TimeDelta::FromDays(advance_warning_days);
    // But, wait an extra hour so that when this code is next run, it is clear
    // we are now inside advance_warning_days (and not right on the boundary).
    recheck_delay += kOneHour;
    RecheckAfter(recheck_delay);
  }
}

void Rechecker::RecheckAfter(base::TimeDelta delay) {
  base::PostDelayedTaskWithTraits(
      FROM_HERE, kRecheckTaskTraits,
      base::BindOnce(&Rechecker::Recheck, task_weak_ptr_factory_.GetWeakPtr()),
      std::max(delay, kOneHour));
  // This always waits at least one hour before calling Recheck again - we don't
  // want some bug to cause this code to run every millisecond.
}

void Rechecker::CancelPendingRecheck() {
  task_weak_ptr_factory_.InvalidateWeakPtrs();
}

void Rechecker::OnLockStateChanged(bool locked) {
  // If a notification is currently showing, we show a new version of it every
  // time the user unlocks the screen. This makes the notification pop up once
  // more - just after typing the password is a good time to remind the user.
  if (!locked && reshow_on_unlock_) {
    Recheck();
  }
}

void Rechecker::Click(const base::Optional<int>& button_index,
                      const base::Optional<base::string16>& reply) {
  bool clicked_on_button = button_index.has_value();
  if (clicked_on_button) {
    PasswordChangeDialog::Show(profile_);
  }
}

void Rechecker::Close(bool by_user) {
  // When a notification is dismissed, we then don't pop it up again each time
  // the user unlocks the screen.
  reshow_on_unlock_ = false;
}

// static
void Rechecker::StartForProfile(Profile* profile) {
  if (!instance_) {
    const AccountId account_id =
        ProfileHelper::Get()->GetUserByProfile(profile)->GetAccountId();
    new Rechecker(profile, account_id);
  }
  DCHECK(instance_ && instance_->profile_ == profile);
  instance_->Recheck();
}

// static
void Rechecker::Stop() {
  delete instance_;
}

// static
Rechecker* Rechecker::instance_ = nullptr;

Rechecker::Rechecker(Profile* profile, const AccountId& account_id)
    : profile_(profile), account_id_(account_id) {
  // There must not be an existing singleton instance.
  DCHECK(!instance_);
  instance_ = this;

  // Add |this| as a SessionActivationObserver to see when the screen is locked.
  auto* session_controller = ash::SessionController::Get();
  if (session_controller) {
    session_controller->AddSessionActivationObserverForAccountId(account_id_,
                                                                 this);
  }
}

Rechecker::~Rechecker() {
  // Remove this as a SessionActivationObserver.
  auto* session_controller = ash::SessionController::Get();
  if (session_controller) {
    session_controller->RemoveSessionActivationObserverForAccountId(account_id_,
                                                                    this);
  }

  // This should still be the singleton instance.
  DCHECK_EQ(this, instance_);
  instance_ = nullptr;
}

}  // namespace

void MaybeShowSamlPasswordExpiryNotification(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (IsEnabledForProfile(profile)) {
    Rechecker::StartForProfile(profile);
  }
}

void ShowSamlPasswordExpiryNotification(Profile* profile,
                                        int less_than_n_days) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ShowNotificationImpl(
      profile, less_than_n_days,
      base::MakeRefCounted<HandleNotificationClickDelegate>(
          base::BindRepeating(&PasswordChangeDialog::Show, profile)));
}

void DismissSamlPasswordExpiryNotification(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  NotificationDisplayServiceFactory::GetForProfile(profile)->Close(
      kNotificationHandlerType, kNotificationId);
}

void SamlPasswordExpiryNotificationTestHelper::ResetForTesting() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  delete Rechecker::instance_;
}

void SamlPasswordExpiryNotificationTestHelper::SimulateUnlockForTesting() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Rechecker::instance_->OnLockStateChanged(/*locked=*/false);
}

}  // namespace chromeos
