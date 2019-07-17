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

void RegisterProfilePrefsForBrowser(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kAssistantConsentStatus,
                                ConsentStatus::kUnknown, PrefRegistry::PUBLIC);
}

void RegisterProfilePrefsForeign(PrefRegistrySimple* registry, bool for_test) {
  if (for_test) {
    // In tests there are no remote pref service. Register the prefs as own if
    // necessary.
    RegisterProfilePrefsForBrowser(registry);
    return;
  }
  registry->RegisterForeignPref(kAssistantConsentStatus);
}

}  // namespace prefs
}  // namespace assistant
}  // namespace chromeos
