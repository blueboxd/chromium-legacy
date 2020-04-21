// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/connectors_prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace enterprise_connectors {

const char kOnFileAttachedPref[] = "enterprise_connectors.on_file_attached";

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kOnFileAttachedPref);
}

}  // namespace enterprise_connectors
