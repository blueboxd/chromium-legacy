// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_ui_features_prefs.h"

#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"

namespace chrome_ui_features_prefs {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kUseLegacyFormControls,
                                /*default_value=*/false);
}

}  // namespace chrome_ui_features_prefs
