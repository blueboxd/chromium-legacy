// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/dns_util.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_features.h"
#include "components/embedder_support/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace chrome_browser_net {

namespace {

const char kAlternateErrorPagesBackup[] = "alternate_error_pages.backup";

}  // namespace

class DNSUtilTest : public testing::Test {
 public:
  void SetUp() override { DisableRedesign(); }

  void EnableRedesign() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kPrivacySettingsRedesign, base::FieldTrialParams());
  }

  void DisableRedesign() { scoped_feature_list_.Reset(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(DNSUtilTest, MigrateDNSProbesPref) {
  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterBooleanPref(
      embedder_support::kAlternateErrorPagesEnabled, true);
  prefs.registry()->RegisterBooleanPref(kAlternateErrorPagesBackup, true);

  const PrefService::Preference* current_pref =
      prefs.FindPreference(embedder_support::kAlternateErrorPagesEnabled);
  const PrefService::Preference* backup_pref =
      prefs.FindPreference(kAlternateErrorPagesBackup);

  // No migration happens if the privacy settings redesign is not enabled.
  MigrateDNSProbesSettingToOrFromBackup(&prefs);
  EXPECT_FALSE(backup_pref->HasUserSetting());

  // The hardcoded default value of TRUE gets correctly migrated.
  EnableRedesign();
  MigrateDNSProbesSettingToOrFromBackup(&prefs);
  EXPECT_FALSE(current_pref->HasUserSetting());
  EXPECT_TRUE(backup_pref->HasUserSetting());
  EXPECT_TRUE(prefs.GetBoolean(kAlternateErrorPagesBackup));

  // And correctly restored.
  DisableRedesign();
  MigrateDNSProbesSettingToOrFromBackup(&prefs);
  EXPECT_TRUE(current_pref->HasUserSetting());
  EXPECT_TRUE(prefs.GetBoolean(embedder_support::kAlternateErrorPagesEnabled));
  EXPECT_FALSE(backup_pref->HasUserSetting());

  // An explicit user value of TRUE will be correctly migrated.
  EnableRedesign();
  prefs.SetBoolean(embedder_support::kAlternateErrorPagesEnabled, true);
  MigrateDNSProbesSettingToOrFromBackup(&prefs);
  EXPECT_FALSE(current_pref->HasUserSetting());
  EXPECT_TRUE(backup_pref->HasUserSetting());
  EXPECT_TRUE(prefs.GetBoolean(kAlternateErrorPagesBackup));

  // And correctly restored.
  DisableRedesign();
  MigrateDNSProbesSettingToOrFromBackup(&prefs);
  EXPECT_TRUE(current_pref->HasUserSetting());
  EXPECT_TRUE(prefs.GetBoolean(embedder_support::kAlternateErrorPagesEnabled));
  EXPECT_FALSE(backup_pref->HasUserSetting());

  // An explicit user value of FALSE will also be correctly migrated.
  EnableRedesign();
  prefs.SetBoolean(embedder_support::kAlternateErrorPagesEnabled, false);
  MigrateDNSProbesSettingToOrFromBackup(&prefs);
  EXPECT_FALSE(current_pref->HasUserSetting());
  EXPECT_TRUE(backup_pref->HasUserSetting());
  EXPECT_FALSE(prefs.GetBoolean(kAlternateErrorPagesBackup));

  // And correctly restored.
  DisableRedesign();
  MigrateDNSProbesSettingToOrFromBackup(&prefs);
  EXPECT_TRUE(current_pref->HasUserSetting());
  EXPECT_FALSE(prefs.GetBoolean(embedder_support::kAlternateErrorPagesEnabled));
  EXPECT_FALSE(backup_pref->HasUserSetting());

  // A policy-sourced value of TRUE takes precedence over the user-sourced value
  // of FALSE when the preference is evaluated. However, it will still be the
  // user-sourced value of FALSE that will be migrated.
  prefs.SetManagedPref(embedder_support::kAlternateErrorPagesEnabled,
                       std::make_unique<base::Value>(true));
  EnableRedesign();
  MigrateDNSProbesSettingToOrFromBackup(&prefs);
  EXPECT_FALSE(current_pref->HasUserSetting());
  EXPECT_TRUE(backup_pref->HasUserSetting());
  EXPECT_FALSE(prefs.GetBoolean(kAlternateErrorPagesBackup));

  // And correctly restored.
  DisableRedesign();
  MigrateDNSProbesSettingToOrFromBackup(&prefs);
  EXPECT_TRUE(current_pref->HasUserSetting());
  {
    const base::Value* user_pref =
        prefs.GetUserPref(embedder_support::kAlternateErrorPagesEnabled);
    ASSERT_TRUE(user_pref->is_bool());
    EXPECT_FALSE(user_pref->GetBool());
  }
  EXPECT_FALSE(backup_pref->HasUserSetting());

  // After clearing the user-sourced value, the hardcoded value of TRUE should
  // be the value which is migrated, even if it is overridden by
  // a policy-sourced value of FALSE.
  prefs.ClearPref(embedder_support::kAlternateErrorPagesEnabled);
  prefs.SetManagedPref(embedder_support::kAlternateErrorPagesEnabled,
                       std::make_unique<base::Value>(false));
  EnableRedesign();
  MigrateDNSProbesSettingToOrFromBackup(&prefs);
  EXPECT_FALSE(current_pref->HasUserSetting());
  EXPECT_TRUE(backup_pref->HasUserSetting());
  EXPECT_TRUE(prefs.GetBoolean(kAlternateErrorPagesBackup));

  // And correctly restored.
  DisableRedesign();
  MigrateDNSProbesSettingToOrFromBackup(&prefs);
  EXPECT_TRUE(current_pref->HasUserSetting());
  {
    const base::Value* user_pref =
        prefs.GetUserPref(embedder_support::kAlternateErrorPagesEnabled);
    ASSERT_TRUE(user_pref->is_bool());
    EXPECT_TRUE(user_pref->GetBool());
  }
  EXPECT_FALSE(backup_pref->HasUserSetting());
}

TEST(DNSUtil, SplitDohTemplateGroup) {
  EXPECT_THAT(SplitDohTemplateGroup("a"), ElementsAre("a"));
  EXPECT_THAT(SplitDohTemplateGroup("a b"), ElementsAre("a", "b"));
  EXPECT_THAT(SplitDohTemplateGroup("a \tb\nc"), ElementsAre("a", "b\nc"));
  EXPECT_THAT(SplitDohTemplateGroup(" \ta b\n"), ElementsAre("a", "b"));
}

TEST(DNSUtil, IsValidDohTemplateGroup) {
  EXPECT_TRUE(IsValidDohTemplateGroup(""));
  EXPECT_TRUE(IsValidDohTemplateGroup("https://valid"));
  EXPECT_TRUE(IsValidDohTemplateGroup("https://valid https://valid2"));

  EXPECT_FALSE(IsValidDohTemplateGroup("https://valid invalid"));
  EXPECT_FALSE(IsValidDohTemplateGroup("invalid https://valid"));
  EXPECT_FALSE(IsValidDohTemplateGroup("invalid"));
  EXPECT_FALSE(IsValidDohTemplateGroup("invalid invalid2"));
}

}  // namespace chrome_browser_net
