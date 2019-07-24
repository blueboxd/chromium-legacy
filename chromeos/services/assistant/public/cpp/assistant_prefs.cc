// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/public/cpp/assistant_prefs.h"

#include <string>

#include "components/prefs/pref_registry_simple.h"

namespace chromeos {
namespace assistant {
namespace prefs {

// A preference that indicates the activity control consent status from user.
// This preference should only be changed in browser.
const char kAssistantConsentStatus[] =
    "settings.voice_interaction.activity_control.consent_status";
// A preference that indicates the Assistant has been disabled by domain policy.
// If true, the Assistant will always been disabled and user cannot enable it.
// This preference should only be changed in browser.
const char kAssistantDisabledByPolicy[] =
    "settings.assistant.disabled_by_policy";
// A preference that indicates the user has chosen to always keep hotword
// listening on even without DSP support.
// This preference should only be changed in browser.
const char kAssistantHotwordAlwaysOn[] =
    "settings.voice_interaction.hotword.always_on";
// A preference that indicates whether microphone should be open when the
// Assistant launches.
// This preference should only be changed in browser.
const char kAssistantLaunchWithMicOpen[] =
    "settings.voice_interaction.launch_with_mic_open";
// A preference that indicates the user has allowed the Assistant services
// to send notification.
// This preference should only be changed in browser.
const char kAssistantNotificationEnabled[] =
    "settings.voice_interaction.notification.enabled";

void RegisterProfilePrefsForBrowser(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kAssistantConsentStatus,
                                ConsentStatus::kUnknown, PrefRegistry::PUBLIC);
  registry->RegisterBooleanPref(kAssistantDisabledByPolicy, false,
                                PrefRegistry::PUBLIC);
  registry->RegisterBooleanPref(kAssistantHotwordAlwaysOn, false,
                                PrefRegistry::PUBLIC);
  registry->RegisterBooleanPref(kAssistantLaunchWithMicOpen, false,
                                PrefRegistry::PUBLIC);
  registry->RegisterBooleanPref(kAssistantNotificationEnabled, true,
                                PrefRegistry::PUBLIC);
}

void RegisterProfilePrefsForeign(PrefRegistrySimple* registry, bool for_test) {
  if (for_test) {
    // In tests there are no remote pref service. Register the prefs as own if
    // necessary.
    RegisterProfilePrefsForBrowser(registry);
    return;
  }
  registry->RegisterForeignPref(kAssistantConsentStatus);
  registry->RegisterForeignPref(kAssistantDisabledByPolicy);
  registry->RegisterForeignPref(kAssistantHotwordAlwaysOn);
  registry->RegisterForeignPref(kAssistantLaunchWithMicOpen);
  registry->RegisterForeignPref(kAssistantNotificationEnabled);
}

}  // namespace prefs
}  // namespace assistant
}  // namespace chromeos
