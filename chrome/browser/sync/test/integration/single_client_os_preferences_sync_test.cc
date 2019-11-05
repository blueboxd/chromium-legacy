// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/shelf_prefs.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/test/integration/preferences_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gtest/include/gtest/gtest.h"

using preferences_helper::ChangeStringPref;
using preferences_helper::StringPrefMatches;

namespace {

class SingleClientOsPreferencesSyncTest : public SyncTest {
 public:
  SingleClientOsPreferencesSyncTest() : SyncTest(SINGLE_CLIENT) {
    settings_feature_list_.InitAndEnableFeature(
        chromeos::features::kSplitSettingsSync);
  }

  ~SingleClientOsPreferencesSyncTest() override = default;

 private:
  // The names |scoped_feature_list_| and |feature_list_| are both used in
  // superclasses.
  base::test::ScopedFeatureList settings_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(SingleClientOsPreferencesSyncTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientOsPreferencesSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Shelf alignment is a Chrome OS only preference.
  ASSERT_TRUE(StringPrefMatches(ash::prefs::kShelfAlignment));
  ChangeStringPref(/*profile_index=*/0, ash::prefs::kShelfAlignment,
                   ash::kShelfAlignmentRight);
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  EXPECT_TRUE(StringPrefMatches(ash::prefs::kShelfAlignment));
}

}  // namespace
