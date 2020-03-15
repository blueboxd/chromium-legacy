// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/test/integration/apps_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/common/chrome_features.h"
#include "components/sync/driver/profile_sync_service.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/sync/test/integration/os_sync_test.h"
#include "chromeos/constants/chromeos_features.h"
#endif

using apps_helper::AllProfilesHaveSameApps;
using apps_helper::InstallHostedApp;
using apps_helper::InstallPlatformApp;

class SingleClientAppsSyncTest
    : public SyncTest,
      public ::testing::WithParamInterface<web_app::ProviderType> {
 public:
  SingleClientAppsSyncTest() : SyncTest(SINGLE_CLIENT) {
    switch (GetParam()) {
      case web_app::ProviderType::kWebApps:
        scoped_feature_list_.InitAndEnableFeature(
            features::kDesktopPWAsWithoutExtensions);
        break;
      case web_app::ProviderType::kBookmarkApps:
        scoped_feature_list_.InitAndDisableFeature(
            features::kDesktopPWAsWithoutExtensions);
        break;
    }
  }

  ~SingleClientAppsSyncTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(SingleClientAppsSyncTest);
};

// crbug.com/1001437
#if defined(ADDRESS_SANITIZER)
#define MAYBE_InstallSomePlatformApps DISABLED_InstallSomePlatformApps
#define MAYBE_InstallSomeApps DISABLED_InstallSomeApps
#else
#define MAYBE_InstallSomePlatformApps InstallSomePlatformApps
#define MAYBE_InstallSomeApps InstallSomeApps
#endif

IN_PROC_BROWSER_TEST_P(SingleClientAppsSyncTest, StartWithNoApps) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameApps());
}

IN_PROC_BROWSER_TEST_P(SingleClientAppsSyncTest, StartWithSomeLegacyApps) {
  ASSERT_TRUE(SetupClients());

  const int kNumApps = 5;
  for (int i = 0; i < kNumApps; ++i) {
    InstallHostedApp(GetProfile(0), i);
    InstallHostedApp(verifier(), i);
  }

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameApps());
}

IN_PROC_BROWSER_TEST_P(SingleClientAppsSyncTest, StartWithSomePlatformApps) {
  ASSERT_TRUE(SetupClients());

  const int kNumApps = 5;
  for (int i = 0; i < kNumApps; ++i) {
    InstallPlatformApp(GetProfile(0), i);
    InstallPlatformApp(verifier(), i);
  }

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameApps());
}

IN_PROC_BROWSER_TEST_P(SingleClientAppsSyncTest, InstallSomeLegacyApps) {
  ASSERT_TRUE(SetupSync());

  const int kNumApps = 5;
  for (int i = 0; i < kNumApps; ++i) {
    InstallHostedApp(GetProfile(0), i);
    InstallHostedApp(verifier(), i);
  }

  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(AllProfilesHaveSameApps());
}

IN_PROC_BROWSER_TEST_P(SingleClientAppsSyncTest,
                       MAYBE_InstallSomePlatformApps) {
  ASSERT_TRUE(SetupSync());

  const int kNumApps = 5;
  for (int i = 0; i < kNumApps; ++i) {
    InstallPlatformApp(GetProfile(0), i);
    InstallPlatformApp(verifier(), i);
  }

  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(AllProfilesHaveSameApps());
}

IN_PROC_BROWSER_TEST_P(SingleClientAppsSyncTest, MAYBE_InstallSomeApps) {
  ASSERT_TRUE(SetupSync());

  int i = 0;

  const int kNumApps = 5;
  for (int j = 0; j < kNumApps; ++i, ++j) {
    InstallHostedApp(GetProfile(0), i);
    InstallHostedApp(verifier(), i);
  }

  const int kNumPlatformApps = 5;
  for (int j = 0; j < kNumPlatformApps; ++i, ++j) {
    InstallPlatformApp(GetProfile(0), i);
    InstallPlatformApp(verifier(), i);
  }

  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(AllProfilesHaveSameApps());
}

#if defined(OS_CHROMEOS)

// Tests for SplitSettingsSync.
class SingleClientAppsOsSyncTest
    : public OsSyncTest,
      public ::testing::WithParamInterface<web_app::ProviderType> {
 public:
  SingleClientAppsOsSyncTest() : OsSyncTest(SINGLE_CLIENT) {
    switch (GetParam()) {
      case web_app::ProviderType::kWebApps:
        scoped_feature_list_.InitAndEnableFeature(
            features::kDesktopPWAsWithoutExtensions);
        break;
      case web_app::ProviderType::kBookmarkApps:
        scoped_feature_list_.InitAndDisableFeature(
            features::kDesktopPWAsWithoutExtensions);
        break;
    }
  }
  ~SingleClientAppsOsSyncTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(SingleClientAppsOsSyncTest);
};

IN_PROC_BROWSER_TEST_P(SingleClientAppsOsSyncTest,
                       DisablingOsSyncFeatureDisablesDataType) {
  ASSERT_TRUE(chromeos::features::IsSplitSettingsSyncEnabled());
  ASSERT_TRUE(SetupSync());
  syncer::SyncService* service = GetSyncService(0);
  syncer::SyncUserSettings* settings = service->GetUserSettings();

  EXPECT_TRUE(settings->IsOsSyncFeatureEnabled());
  EXPECT_TRUE(service->GetActiveDataTypes().Has(syncer::APPS));

  settings->SetOsSyncFeatureEnabled(false);
  EXPECT_FALSE(settings->IsOsSyncFeatureEnabled());
  EXPECT_FALSE(service->GetActiveDataTypes().Has(syncer::APPS));
}

INSTANTIATE_TEST_SUITE_P(All,
                         SingleClientAppsOsSyncTest,
                         ::testing::Values(web_app::ProviderType::kBookmarkApps,
                                           web_app::ProviderType::kWebApps),
                         web_app::ProviderTypeParamToString);

#endif  // defined(OS_CHROMEOS)

INSTANTIATE_TEST_SUITE_P(All,
                         SingleClientAppsSyncTest,
                         ::testing::Values(web_app::ProviderType::kBookmarkApps,
                                           web_app::ProviderType::kWebApps),
                         web_app::ProviderTypeParamToString);
