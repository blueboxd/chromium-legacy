// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/quiet_notification_permission_ui_state.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

// static
void QuietNotificationPermissionUiState::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kNotificationPermissionActions,
                             PrefRegistry::LOSSY_PREF);
  // TODO(crbug.com/1001857): Consider making this syncable.
  registry->RegisterBooleanPref(prefs::kEnableQuietNotificationPermissionUi,
                                false /* default_value */);
  registry->RegisterBooleanPref(
      prefs::kQuietNotificationPermissionShouldShowPromo,
      false /* default_value */);
  registry->RegisterBooleanPref(
      prefs::kQuietNotificationPermissionPromoWasShown,
      false /* default_value */);
}

// static
bool QuietNotificationPermissionUiState::ShouldShowPromo(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(
             prefs::kEnableQuietNotificationPermissionUi) &&
         profile->GetPrefs()->GetBoolean(
             prefs::kQuietNotificationPermissionShouldShowPromo) &&
         !profile->GetPrefs()->GetBoolean(
             prefs::kQuietNotificationPermissionPromoWasShown);
}

// static
void QuietNotificationPermissionUiState::PromoWasShown(Profile* profile) {
  profile->GetPrefs()->SetBoolean(
      prefs::kQuietNotificationPermissionPromoWasShown, true /* value */);
}
