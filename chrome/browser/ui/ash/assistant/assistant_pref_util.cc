// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/assistant_pref_util.h"

#include <string>

#include "components/arc/arc_prefs.h"
#include "components/prefs/pref_registry_simple.h"

namespace assistant {
namespace prefs {

// A preference that indicates the Assistant has been disabled by domain policy.
// If true, the Assistant will always been disabled and user cannot enable it.
const char kAssistantDisabledByPolicy[] =
    "settings.assistant.disabled_by_policy";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kAssistantDisabledByPolicy, false);
}

}  // namespace prefs
}  // namespace assistant
