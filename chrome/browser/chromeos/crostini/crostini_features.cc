// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_features.h"

#include "base/feature_list.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"

namespace crostini {

static CrostiniFeatures* g_crostini_features = nullptr;

CrostiniFeatures* CrostiniFeatures::Get() {
  if (!g_crostini_features) {
    g_crostini_features = new CrostiniFeatures();
  }
  return g_crostini_features;
}

void CrostiniFeatures::SetForTesting(CrostiniFeatures* features) {
  g_crostini_features = features;
}

CrostiniFeatures::CrostiniFeatures() = default;

CrostiniFeatures::~CrostiniFeatures() = default;

bool CrostiniFeatures::IsUIAllowed(Profile* profile, bool check_policy) {
  // TODO(crbug.com/1004708): Move implementation from crostini_util to here and
  // redirect all callers.
  return crostini::IsCrostiniUIAllowedForProfile(profile);
}

bool CrostiniFeatures::IsExportImportUIAllowed(Profile* profile) {
  return g_crostini_features->IsUIAllowed(profile, true) &&
         base::FeatureList::IsEnabled(chromeos::features::kCrostiniBackup) &&
         profile->GetPrefs()->GetBoolean(
             crostini::prefs::kUserCrostiniExportImportUIAllowedByPolicy);
}

bool CrostiniFeatures::IsRootAccessAllowed(Profile* profile) {
  // TODO(crbug.com/1004708): Move implementation from crostini_util to here and
  // redirect all callers.
  return crostini::IsCrostiniRootAccessAllowed(profile);
}

}  // namespace crostini
