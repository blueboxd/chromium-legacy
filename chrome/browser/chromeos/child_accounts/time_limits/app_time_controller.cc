// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_controller.h"

#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_activity_registry.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_service_wrapper.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_limit_utils.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_limits_whitelist_policy_wrapper.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_policy_helpers.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_types.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/web_time_activity_provider.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/web_time_limit_enforcer.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace chromeos {
namespace app_time {

namespace {

constexpr base::TimeDelta kDay = base::TimeDelta::FromHours(24);

// Family link notifier id.
constexpr char kFamilyLinkSourceId[] = "family-link";

// Time limit reaching id. This id will be appended by the application name. The
// 5 minute and one minute notifications for the same app will have the same id.
constexpr char kAppTimeLimitReachingNotificationId[] =
    "time-limit-reaching-id-";

// Time limit updated id. This id will be appended by the application name.
constexpr char kAppTimeLimitUpdateNotificationId[] = "time-limit-updated-id-";

// Used to convert |time_limit| to string. |cutoff| specifies whether the
// formatted result will be one value or two values: for example if the time
// delta is 2 hours 30 minutes: |cutoff| of <2 will result in "2 hours" and
// |cutoff| of 3 will result in "2 hours and 30 minutes".
base::string16 GetTimeLimitMessage(base::TimeDelta time_limit, int cutoff) {
  return ui::TimeFormat::Detailed(ui::TimeFormat::Format::FORMAT_DURATION,
                                  ui::TimeFormat::Length::LENGTH_LONG, cutoff,
                                  time_limit);
}

base::string16 GetNotificationTitleFor(
    const base::string16& app_name,
    chromeos::app_time::AppNotification notification) {
  switch (notification) {
    case chromeos::app_time::AppNotification::kFiveMinutes:
    case chromeos::app_time::AppNotification::kOneMinute:
      return l10n_util::GetStringFUTF16(
          IDS_APP_TIME_LIMIT_APP_WILL_PAUSE_SYSTEM_NOTIFICATION_TITLE,
          app_name);
    case chromeos::app_time::AppNotification::kTimeLimitChanged:
      return l10n_util::GetStringUTF16(
          IDS_APP_TIME_LIMIT_APP_TIME_LIMIT_SET_SYSTEM_NOTIFICATION_TITLE);
    default:
      NOTREACHED();
      return base::EmptyString16();
  }
}

base::string16 GetNotificationMessageFor(
    const base::string16& app_name,
    chromeos::app_time::AppNotification notification,
    base::TimeDelta time_limit) {
  switch (notification) {
    case chromeos::app_time::AppNotification::kFiveMinutes:
      return l10n_util::GetStringFUTF16(
          IDS_APP_TIME_LIMIT_APP_WILL_PAUSE_SYSTEM_NOTIFICATION_MESSAGE,
          GetTimeLimitMessage(base::TimeDelta::FromMinutes(5), /* cutoff */ 1));
    case chromeos::app_time::AppNotification::kOneMinute:
      return l10n_util::GetStringFUTF16(
          IDS_APP_TIME_LIMIT_APP_WILL_PAUSE_SYSTEM_NOTIFICATION_MESSAGE,
          GetTimeLimitMessage(base::TimeDelta::FromMinutes(1), /* cutoff */ 1));
    case chromeos::app_time::AppNotification::kTimeLimitChanged:
      return l10n_util::GetStringFUTF16(
          IDS_APP_TIME_LIMIT_APP_TIME_LIMIT_SET_SYSTEM_NOTIFICATION_MESSAGE,
          GetTimeLimitMessage(time_limit, /* cutoff */ 3), app_name);
    default:
      NOTREACHED();
      return base::EmptyString16();
  }
}

std::string GetNotificationIdFor(
    const std::string& app_name,
    chromeos::app_time::AppNotification notification) {
  std::string notification_id;
  switch (notification) {
    case chromeos::app_time::AppNotification::kFiveMinutes:
    case chromeos::app_time::AppNotification::kOneMinute:
      notification_id = kAppTimeLimitReachingNotificationId;
      break;
    case chromeos::app_time::AppNotification::kTimeLimitChanged:
      notification_id = kAppTimeLimitUpdateNotificationId;
      break;
    default:
      NOTREACHED();
      notification_id = "";
      break;
  }
  return base::StrCat({notification_id, app_name});
}

void ShowNotificationForApp(const std::string& app_name,
                            chromeos::app_time::AppNotification notification,
                            base::TimeDelta time_limit,
                            Profile* profile,
                            base::Optional<gfx::ImageSkia> icon) {
  DCHECK(notification == chromeos::app_time::AppNotification::kFiveMinutes ||
         notification == chromeos::app_time::AppNotification::kOneMinute ||
         notification ==
             chromeos::app_time::AppNotification::kTimeLimitChanged);

  // Alright we have all the messages that we want.
  const base::string16 app_name_16 = base::UTF8ToUTF16(app_name);
  const base::string16 title =
      GetNotificationTitleFor(app_name_16, notification);
  const base::string16 message =
      GetNotificationMessageFor(app_name_16, notification, time_limit);
  // Family link display source.
  const base::string16 notification_source =
      l10n_util::GetStringUTF16(IDS_TIME_LIMIT_NOTIFICATION_DISPLAY_SOURCE);

  std::string notification_id = GetNotificationIdFor(app_name, notification);

  std::unique_ptr<message_center::Notification> message_center_notification =
      ash::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, notification_id, title,
          message, notification_source, GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kFamilyLinkSourceId),
          message_center::RichNotificationData(),
          base::MakeRefCounted<message_center::NotificationDelegate>(),
          ash::kNotificationSupervisedUserIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);

  if (icon.has_value())
    message_center_notification->set_icon(gfx::Image(icon.value()));

  auto* notification_display_service =
      NotificationDisplayService::GetForProfile(profile);
  if (!notification_display_service)
    return;

  notification_display_service->Display(NotificationHandler::Type::TRANSIENT,
                                        *message_center_notification,
                                        /*metadata=*/nullptr);
}

}  // namespace

AppTimeController::TestApi::TestApi(AppTimeController* controller)
    : controller_(controller) {}

AppTimeController::TestApi::~TestApi() = default;

void AppTimeController::TestApi::SetLastResetTime(base::Time time) {
  controller_->SetLastResetTime(time);
}

base::Time AppTimeController::TestApi::GetNextResetTime() const {
  return controller_->GetNextResetTime();
}

base::Time AppTimeController::TestApi::GetLastResetTime() const {
  return controller_->last_limits_reset_time_.value();
}

AppActivityRegistry* AppTimeController::TestApi::app_registry() {
  return controller_->app_registry_.get();
}

// static
bool AppTimeController::ArePerAppTimeLimitsEnabled() {
  return base::FeatureList::IsEnabled(features::kPerAppTimeLimits);
}

bool AppTimeController::IsAppActivityReportingEnabled() {
  return base::FeatureList::IsEnabled(features::kAppActivityReporting);
}

// static
void AppTimeController::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kPerAppTimeLimitsPolicy);
  registry->RegisterDictionaryPref(prefs::kPerAppTimeLimitsWhitelistPolicy);
}

AppTimeController::AppTimeController(Profile* profile)
    : profile_(profile),
      app_service_wrapper_(std::make_unique<AppServiceWrapper>(profile)),
      app_registry_(
          std::make_unique<AppActivityRegistry>(app_service_wrapper_.get(),
                                                this)),
      web_time_activity_provider_(std::make_unique<WebTimeActivityProvider>(
          this,
          app_service_wrapper_.get())) {
  DCHECK(profile);

  if (WebTimeLimitEnforcer::IsEnabled())
    web_time_enforcer_ = std::make_unique<WebTimeLimitEnforcer>(this);

  app_registry_->AddAppStateObserver(this);

  PrefService* pref_service = profile->GetPrefs();
  RegisterProfilePrefObservers(pref_service);

  // TODO: Update the following from user pref.instead of setting it to Now().
  SetLastResetTime(base::Time::Now());

  if (HasTimeCrossedResetBoundary()) {
    OnResetTimeReached();
  } else {
    ScheduleForTimeLimitReset();
  }

  auto* system_clock_client = SystemClockClient::Get();
  // SystemClockClient may not be initialized in some tests.
  if (system_clock_client)
    system_clock_client->AddObserver(this);

  auto* time_zone_settings = system::TimezoneSettings::GetInstance();
  if (time_zone_settings)
    time_zone_settings->AddObserver(this);

  // Update app limits and reset time from policy.
  TimeLimitsPolicyUpdated(prefs::kPerAppTimeLimitsPolicy);
}

AppTimeController::~AppTimeController() {
  app_registry_->RemoveAppStateObserver(this);

  auto* time_zone_settings = system::TimezoneSettings::GetInstance();
  if (time_zone_settings)
    time_zone_settings->RemoveObserver(this);

  auto* system_clock_client = SystemClockClient::Get();
  if (system_clock_client)
    system_clock_client->RemoveObserver(this);
}

bool AppTimeController::IsExtensionWhitelisted(
    const std::string& extension_id) const {
  NOTIMPLEMENTED();

  return true;
}

void AppTimeController::SystemClockUpdated() {
  if (HasTimeCrossedResetBoundary())
    OnResetTimeReached();
}

void AppTimeController::TimezoneChanged(const icu::TimeZone& timezone) {
  // Timezone changes may not require us to reset information,
  // however, they may require updating the scheduled reset time.
  ScheduleForTimeLimitReset();
}

void AppTimeController::RegisterProfilePrefObservers(
    PrefService* pref_service) {
  pref_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_registrar_->Init(pref_service);

  // Adds callbacks to observe policy pref changes.
  // Using base::Unretained(this) is safe here because when |pref_registrar_|
  // gets destroyed, it will remove the observers from PrefService.
  pref_registrar_->Add(
      prefs::kPerAppTimeLimitsPolicy,
      base::BindRepeating(&AppTimeController::TimeLimitsPolicyUpdated,
                          base::Unretained(this)));
  pref_registrar_->Add(
      prefs::kPerAppTimeLimitsWhitelistPolicy,
      base::BindRepeating(&AppTimeController::TimeLimitsWhitelistPolicyUpdated,
                          base::Unretained(this)));
}

void AppTimeController::TimeLimitsPolicyUpdated(const std::string& pref_name) {
  DCHECK_EQ(pref_name, prefs::kPerAppTimeLimitsPolicy);

  const base::Value* policy =
      pref_registrar_->prefs()->GetDictionary(prefs::kPerAppTimeLimitsPolicy);

  if (!policy || !policy->is_dict()) {
    LOG(WARNING) << "Invalid PerAppTimeLimits policy.";
    return;
  }

  app_registry_->UpdateAppLimits(policy::AppLimitsFromDict(*policy));

  base::Optional<base::TimeDelta> new_reset_time =
      policy::ResetTimeFromDict(*policy);
  // TODO(agawronska): Propagate the information about reset time change.
  if (new_reset_time && *new_reset_time != limits_reset_time_)
    limits_reset_time_ = *new_reset_time;
}

void AppTimeController::TimeLimitsWhitelistPolicyUpdated(
    const std::string& pref_name) {
  DCHECK_EQ(pref_name, prefs::kPerAppTimeLimitsWhitelistPolicy);

  const base::DictionaryValue* policy = pref_registrar_->prefs()->GetDictionary(
      prefs::kPerAppTimeLimitsWhitelistPolicy);

  // Figure out a way to avoid cloning
  AppTimeLimitsWhitelistPolicyWrapper wrapper(policy);

  app_registry_->OnTimeLimitWhitelistChanged(wrapper);

  if (web_time_enforcer_)
    web_time_enforcer_->OnTimeLimitWhitelistChanged(wrapper);
}

void AppTimeController::ShowAppTimeLimitNotification(
    const AppId& app_id,
    base::TimeDelta time_limit,
    AppNotification notification) {
  switch (notification) {
    case AppNotification::kFiveMinutes:
    case AppNotification::kOneMinute:
    case AppNotification::kTimeLimitChanged: {
      std::string app_name = app_service_wrapper_->GetAppName(app_id);
      int size_hint_in_dp = 48;
      app_service_wrapper_->GetAppIcon(
          app_id, size_hint_in_dp,
          base::BindOnce(&ShowNotificationForApp, app_name, notification,
                         time_limit, profile_));
      return;
    }
    case AppNotification::kTimeLimitReached:
      // TODO(1015658)
      return;
    default:
      return;
  }
}

void AppTimeController::OnAppLimitReached(const AppId& app_id,
                                          base::TimeDelta time_limit,
                                          bool was_active) {
  app_service_wrapper_->PauseApp(app_id, time_limit, was_active);
}

void AppTimeController::OnAppLimitRemoved(const AppId& app_id) {
  app_service_wrapper_->ResumeApp(app_id);
}

void AppTimeController::OnAppInstalled(const AppId& app_id) {
  const base::Value* policy =
      pref_registrar_->prefs()->GetDictionary(prefs::kPerAppTimeLimitsPolicy);

  if (!policy || !policy->is_dict()) {
    LOG(WARNING) << "Invalid PerAppTimeLimits policy.";
    return;
  }

  const std::map<AppId, AppLimit> limits = policy::AppLimitsFromDict(*policy);
  // Update the limit for newly installed app, if it exists.
  auto result = limits.find(app_id);
  if (result == limits.end())
    return;

  app_registry_->SetAppLimit(result->first, result->second);
}

base::Time AppTimeController::GetNextResetTime() const {
  // UTC time now.
  base::Time now = base::Time::Now();

  // UTC time local midnight.
  base::Time nearest_midnight = now.LocalMidnight();

  base::Time prev_midnight;
  if (now > nearest_midnight)
    prev_midnight = nearest_midnight;
  else
    prev_midnight = nearest_midnight - base::TimeDelta::FromHours(24);

  base::Time next_reset_time = prev_midnight + limits_reset_time_;

  if (next_reset_time > now)
    return next_reset_time;

  // We have already reset for this day. The reset time is the next day.
  return next_reset_time + base::TimeDelta::FromHours(24);
}

void AppTimeController::ScheduleForTimeLimitReset() {
  if (reset_timer_.IsRunning())
    reset_timer_.AbandonAndStop();

  base::TimeDelta time_until_reset = GetNextResetTime() - base::Time::Now();
  reset_timer_.Start(FROM_HERE, time_until_reset,
                     base::BindOnce(&AppTimeController::OnResetTimeReached,
                                    base::Unretained(this)));
}

void AppTimeController::OnResetTimeReached() {
  base::Time now = base::Time::Now();

  app_registry_->OnResetTimeReached(now);

  SetLastResetTime(now);

  ScheduleForTimeLimitReset();
}

void AppTimeController::SetLastResetTime(base::Time timestamp) {
  last_limits_reset_time_ = timestamp;
  // TODO(crbug.com/1015658) : |last_limits_reset_time_| should be persisted
  // across sessions.
}

bool AppTimeController::HasTimeCrossedResetBoundary() const {
  // last_limits_reset_time_ doesn't have a value yet. This may be because
  // it has not yet been populated yet (it has not been restored yet).
  if (!last_limits_reset_time_.has_value())
    return false;

  // Time after system time or timezone changed.
  base::Time now = base::Time::Now();
  base::Time last_reset = last_limits_reset_time_.value();

  return now < last_reset || now >= kDay + last_reset;
}

}  // namespace app_time
}  // namespace chromeos
