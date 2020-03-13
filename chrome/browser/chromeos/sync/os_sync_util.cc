// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/sync/os_sync_util.h"

#include "base/metrics/histogram_functions.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/pref_names.h"

namespace os_sync_util {
namespace {

// Returns true if the prefs were migrated.
bool MaybeMigratePreferences(PrefService* prefs) {
  // SplitSyncConsent has its own OOBE flow to enable OS sync, so it doesn't
  // need this migration.
  if (chromeos::features::IsSplitSyncConsentEnabled())
    return false;

  // Migration code can be removed when SplitSettingsSync has been fully
  // deployed to stable channel, likely in July 2020. When doing this, change
  // the pref kOsSyncFeatureEnabled to default to true and delete the pref
  // kOsSyncPrefsMigrated.
  if (!chromeos::features::IsSplitSettingsSyncEnabled()) {
    // Reset the migration flag because this might be a rollback of the feature.
    // We want migration to happen again when the feature is enabled.
    prefs->SetBoolean(syncer::prefs::kOsSyncPrefsMigrated, false);

    // Reset the OS sync feature just to be safe.
    prefs->SetBoolean(syncer::prefs::kOsSyncFeatureEnabled, false);
    return false;
  }

  // Don't migrate more than once.
  if (prefs->GetBoolean(syncer::prefs::kOsSyncPrefsMigrated))
    return false;

  // Browser sync-the-feature is always enabled on Chrome OS, so OS sync is too.
  prefs->SetBoolean(syncer::prefs::kOsSyncFeatureEnabled, true);

  // OS sync model types get their initial state from the corresponding browser
  // model types.
  prefs->SetBoolean(
      syncer::prefs::kSyncAllOsTypes,
      prefs->GetBoolean(syncer::prefs::kSyncKeepEverythingSynced));
  prefs->SetBoolean(syncer::prefs::kSyncOsApps,
                    prefs->GetBoolean(syncer::prefs::kSyncApps));
  prefs->SetBoolean(syncer::prefs::kSyncOsPreferences,
                    prefs->GetBoolean(syncer::prefs::kSyncPreferences));

  prefs->SetBoolean(syncer::prefs::kOsSyncPrefsMigrated, true);
  return true;
}

}  // namespace

void MigrateOsSyncPreferences(PrefService* prefs) {
  bool migrated = MaybeMigratePreferences(prefs);
  base::UmaHistogramBoolean("ChromeOS.Sync.PreferencesMigrated", migrated);
}

}  // namespace os_sync_util
