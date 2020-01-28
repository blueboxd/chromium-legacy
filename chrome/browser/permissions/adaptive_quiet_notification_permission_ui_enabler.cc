// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/adaptive_quiet_notification_permission_ui_enabler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/containers/adapters.h"
#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"
#include "base/time/default_clock.h"
#include "base/util/values/values_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_config.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/permissions/permission_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace {

// Enable the quiet UX after 3 consecutive denies in adapative activation mode.
constexpr int kConsecutiveDeniesThresholdForActivation = 3u;

// Inner structure of |prefs::kNotificationPermissionActions| containing a
// history of past permission actions. It is a JSON list with the format:
//
//   "profile.content_settings.permission_actions.notifications": [
//     { "time": "1333333333337", "action": 1 },
//     { "time": "1567957177000", "action": 3 },
//     ...
//   ]
//
constexpr char kPermissionActionEntryActionKey[] = "action";
constexpr char kPermissionActionEntryTimestampKey[] = "time";

// Entries in permission actions expire after they become this old.
constexpr base::TimeDelta kPermissionActionMaxAge =
    base::TimeDelta::FromDays(90);

constexpr char kDidAdaptivelyEnableQuietUiInPrefs[] =
    "Permissions.QuietNotificationPrompts.DidEnableAdapativelyInPrefs";
constexpr char kIsQuietUiEnabledInPrefs[] =
    "Permissions.QuietNotificationPrompts.IsEnabledInPrefs";
constexpr char kQuietUiEnabledStateInPrefsChangedTo[] =
    "Permissions.QuietNotificationPrompts.EnabledStateInPrefsChangedTo";

}  // namespace

// AdaptiveQuietNotificationPermissionUiEnabler::Factory ---------------------

// static
AdaptiveQuietNotificationPermissionUiEnabler*
AdaptiveQuietNotificationPermissionUiEnabler::Factory::GetForProfile(
    Profile* profile) {
  return static_cast<AdaptiveQuietNotificationPermissionUiEnabler*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AdaptiveQuietNotificationPermissionUiEnabler::Factory*
AdaptiveQuietNotificationPermissionUiEnabler::Factory::GetInstance() {
  return base::Singleton<
      AdaptiveQuietNotificationPermissionUiEnabler::Factory>::get();
}

AdaptiveQuietNotificationPermissionUiEnabler::Factory::Factory()
    : BrowserContextKeyedServiceFactory(
          "AdaptiveQuietNotificationPermissionUiEnabler",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

AdaptiveQuietNotificationPermissionUiEnabler::Factory::~Factory() {}

KeyedService*
AdaptiveQuietNotificationPermissionUiEnabler::Factory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new AdaptiveQuietNotificationPermissionUiEnabler(
      static_cast<Profile*>(context));
}

content::BrowserContext*
AdaptiveQuietNotificationPermissionUiEnabler::Factory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

// AdaptiveQuietNotificationPermissionUiEnabler ------------------------------

// static
AdaptiveQuietNotificationPermissionUiEnabler*
AdaptiveQuietNotificationPermissionUiEnabler::GetForProfile(Profile* profile) {
  return AdaptiveQuietNotificationPermissionUiEnabler::Factory::GetForProfile(
      profile);
}

void AdaptiveQuietNotificationPermissionUiEnabler::
    RecordPermissionPromptOutcome(permissions::PermissionAction action) {
  ListPrefUpdate update(profile_->GetPrefs(),
                        prefs::kNotificationPermissionActions);
  // Discard permission actions older than |kPermissionActionMaxAge|.
  const base::Time cutoff = clock_->Now() - kPermissionActionMaxAge;
  update->EraseListValueIf([cutoff](const base::Value& entry) {
    const base::Optional<base::Time> timestamp =
        util::ValueToTime(entry.FindKey(kPermissionActionEntryTimestampKey));
    return !timestamp || *timestamp < cutoff;
  });

  // Record the new permission action.
  base::DictionaryValue new_action_attributes;
  new_action_attributes.SetKey(kPermissionActionEntryTimestampKey,
                               util::TimeToValue(clock_->Now()));
  new_action_attributes.SetIntKey(kPermissionActionEntryActionKey,
                                  static_cast<int>(action));
  update->Append(std::move(new_action_attributes));

  // If adaptive activation is disabled, or if the quiet UI is already active,
  // nothing else to do.
  if (!QuietNotificationPermissionUiConfig::IsAdaptiveActivationEnabled() ||
      profile_->GetPrefs()->GetBoolean(
          prefs::kEnableQuietNotificationPermissionUi)) {
    return;
  }

  // Otherwise, turn on quiet UI if the last three permission decision (ignoring
  // dismisses and ignores) were all denies.
  size_t rolling_denies_in_a_row = 0u;
  bool recently_accepted_prompt = false;

  base::Value::ConstListView permission_actions = update->GetList();
  for (const auto& action : base::Reversed(permission_actions)) {
    const base::Optional<int> past_action_as_int =
        action.FindIntKey(kPermissionActionEntryActionKey);
    DCHECK(past_action_as_int);

    const permissions::PermissionAction past_action =
        static_cast<permissions::PermissionAction>(*past_action_as_int);

    switch (past_action) {
      case permissions::PermissionAction::DENIED:
        ++rolling_denies_in_a_row;
        break;
      case permissions::PermissionAction::GRANTED:
        recently_accepted_prompt = true;
        break;
      case permissions::PermissionAction::DISMISSED:
      case permissions::PermissionAction::IGNORED:
      case permissions::PermissionAction::REVOKED:
      default:
        // Ignored.
        break;
    }

    if (rolling_denies_in_a_row >= kConsecutiveDeniesThresholdForActivation) {
      // Set |is_enabling_adaptively_| for the duration of the pref update to
      // inform OnQuietUiStateChanged() that the quiet UI is being enabled
      // adaptively, so that it can record the correct metrics.
      is_enabling_adaptively_ = true;
      profile_->GetPrefs()->SetBoolean(
          prefs::kEnableQuietNotificationPermissionUi, true /* value */);
      profile_->GetPrefs()->SetBoolean(
          prefs::kQuietNotificationPermissionShouldShowPromo, true /* value */);
      is_enabling_adaptively_ = false;
      break;
    }

    if (recently_accepted_prompt)
      break;
  }
}

void AdaptiveQuietNotificationPermissionUiEnabler::ClearInteractionHistory(
    const base::Time& delete_begin,
    const base::Time& delete_end) {
  DCHECK(!delete_end.is_null());

  if (delete_begin.is_null() && delete_end.is_max()) {
    profile_->GetPrefs()->ClearPref(prefs::kNotificationPermissionActions);
    return;
  }

  ListPrefUpdate update(profile_->GetPrefs(),
                        prefs::kNotificationPermissionActions);

  update->EraseListValueIf([delete_begin, delete_end](const auto& entry) {
    const base::Optional<base::Time> timestamp =
        util::ValueToTime(entry.FindKey(kPermissionActionEntryTimestampKey));
    return (!timestamp ||
            (*timestamp >= delete_begin && *timestamp < delete_end));
  });
}

AdaptiveQuietNotificationPermissionUiEnabler::
    AdaptiveQuietNotificationPermissionUiEnabler(Profile* profile)
    : profile_(profile), clock_(base::DefaultClock::GetInstance()) {
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(profile_->GetPrefs());
  pref_change_registrar_->Add(
      prefs::kEnableQuietNotificationPermissionUi,
      base::BindRepeating(
          &AdaptiveQuietNotificationPermissionUiEnabler::OnQuietUiStateChanged,
          base::Unretained(this)));

  // Record whether the quiet UI is enabled, but only when notifications are not
  // completely blocked.
  auto* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  const ContentSetting notifications_setting =
      host_content_settings_map->GetDefaultContentSetting(
          ContentSettingsType::NOTIFICATIONS, nullptr /* provider_id */);
  if (notifications_setting != CONTENT_SETTING_BLOCK) {
    const bool is_quiet_ui_enabled_in_prefs = profile_->GetPrefs()->GetBoolean(
        prefs::kEnableQuietNotificationPermissionUi);
    base::UmaHistogramBoolean(kIsQuietUiEnabledInPrefs,
                              is_quiet_ui_enabled_in_prefs);
  }
}

AdaptiveQuietNotificationPermissionUiEnabler::
    ~AdaptiveQuietNotificationPermissionUiEnabler() = default;

void AdaptiveQuietNotificationPermissionUiEnabler::OnQuietUiStateChanged() {
  const bool is_quiet_ui_enabled_in_prefs = profile_->GetPrefs()->GetBoolean(
      prefs::kEnableQuietNotificationPermissionUi);
  base::UmaHistogramBoolean(kQuietUiEnabledStateInPrefsChangedTo,
                            is_quiet_ui_enabled_in_prefs);

  if (is_quiet_ui_enabled_in_prefs) {
    base::UmaHistogramBoolean(kDidAdaptivelyEnableQuietUiInPrefs,
                              is_enabling_adaptively_);
  } else {
    // Reset the promo state so that if the quiet UI is enabled adaptively
    // again, the promo will be shown again.
    profile_->GetPrefs()->SetBoolean(
        prefs::kQuietNotificationPermissionShouldShowPromo, false /* value */);
    profile_->GetPrefs()->SetBoolean(
        prefs::kQuietNotificationPermissionPromoWasShown, false /* value */);

    // If the users has just turned off the quiet UI, clear interaction history
    // so that if we are in adaptive mode, and the triggering conditions are
    // met, we won't turn it back on immediately.
    ClearInteractionHistory(base::Time(), base::Time::Max());
  }
}
