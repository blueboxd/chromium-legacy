// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_BUDGET_PRIVACY_BUDGET_PREFS_H_
#define CHROME_BROWSER_PRIVACY_BUDGET_PRIVACY_BUDGET_PREFS_H_

#include "components/prefs/pref_registry_simple.h"

namespace prefs {

// Pref used for persisting |IdentifiabilityStudySettings::active_surfaces_|.
extern const char kPrivacyBudgetActiveSurfaces[];

// Pref used for persisting |IdentifiabilityStudySettings::retired_surfaces_|.
extern const char kPrivacyBudgetRetiredSurfaces[];

// Pref used for persisting |IdentifiabilityStudySettings::prng_seed_|.
extern const char kPrivacyBudgetSeed[];

// Pref used for persisting |IdentifiabilityStudySettings::generation_|.
extern const char kPrivacyBudgetGeneration[];

void RegisterPrivacyBudgetPrefs(PrefRegistrySimple* registry);

}  // namespace prefs

#endif  // CHROME_BROWSER_PRIVACY_BUDGET_PRIVACY_BUDGET_PREFS_H_
