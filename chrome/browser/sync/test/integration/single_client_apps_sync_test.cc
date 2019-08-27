// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "chrome/browser/sync/test/integration/apps_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/sync/driver/profile_sync_service.h"

namespace {

using apps_helper::AllProfilesHaveSameApps;
using apps_helper::InstallApp;
using apps_helper::InstallPlatformApp;

class SingleClientAppsSyncTest : public SyncTest {
 public:
  SingleClientAppsSyncTest() : SyncTest(SINGLE_CLIENT) {}

  ~SingleClientAppsSyncTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientAppsSyncTest);
};

// crbug.com/997984
#if defined(MEMORY_SANITIZER) || defined(ADDRESS_SANITIZER)
#define MAYBE_StartWithNoApps DISABLED_StartWithNoApps
#define MAYBE_StartWithSomeLegacyApps DISABLED_StartWithSomeLegacyApps
#define MAYBE_StartWithSomePlatformApps DISABLED_StartWithSomePlatformApps
#define MAYBE_InstallSomeLegacyApps DISABLED_InstallSomeLegacyApps
#define MAYBE_InstallSomePlatformApps DISABLED_InstallSomePlatformApps
#define MAYBE_InstallSomeApps DISABLED_InstallSomeApps
#else
#define MAYBE_StartWithNoApps StartWithNoApps
#define MAYBE_StartWithSomeLegacyApps StartWithSomeLegacyApps
#define MAYBE_StartWithSomePlatformApps StartWithSomePlatformApps
#define MAYBE_InstallSomeLegacyApps InstallSomeLegacyApps
#define MAYBE_InstallSomePlatformApps InstallSomePlatformApps
#define MAYBE_InstallSomeApps InstallSomeApps
#endif

IN_PROC_BROWSER_TEST_F(SingleClientAppsSyncTest, MAYBE_StartWithNoApps) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameApps());
}

IN_PROC_BROWSER_TEST_F(SingleClientAppsSyncTest, MAYBE_StartWithSomeLegacyApps) {
  ASSERT_TRUE(SetupClients());

  const int kNumApps = 5;
  for (int i = 0; i < kNumApps; ++i) {
    InstallApp(GetProfile(0), i);
    InstallApp(verifier(), i);
  }

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameApps());
}

IN_PROC_BROWSER_TEST_F(SingleClientAppsSyncTest, MAYBE_StartWithSomePlatformApps) {
  ASSERT_TRUE(SetupClients());

  const int kNumApps = 5;
  for (int i = 0; i < kNumApps; ++i) {
    InstallPlatformApp(GetProfile(0), i);
    InstallPlatformApp(verifier(), i);
  }

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameApps());
}

IN_PROC_BROWSER_TEST_F(SingleClientAppsSyncTest, MAYBE_InstallSomeLegacyApps) {
  ASSERT_TRUE(SetupSync());

  const int kNumApps = 5;
  for (int i = 0; i < kNumApps; ++i) {
    InstallApp(GetProfile(0), i);
    InstallApp(verifier(), i);
  }

  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(AllProfilesHaveSameApps());
}

IN_PROC_BROWSER_TEST_F(SingleClientAppsSyncTest, MAYBE_InstallSomePlatformApps) {
  ASSERT_TRUE(SetupSync());

  const int kNumApps = 5;
  for (int i = 0; i < kNumApps; ++i) {
    InstallPlatformApp(GetProfile(0), i);
    InstallPlatformApp(verifier(), i);
  }

  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(AllProfilesHaveSameApps());
}

IN_PROC_BROWSER_TEST_F(SingleClientAppsSyncTest, MAYBE_InstallSomeApps) {
  ASSERT_TRUE(SetupSync());

  int i = 0;

  const int kNumApps = 5;
  for (int j = 0; j < kNumApps; ++i, ++j) {
    InstallApp(GetProfile(0), i);
    InstallApp(verifier(), i);
  }

  const int kNumPlatformApps = 5;
  for (int j = 0; j < kNumPlatformApps; ++i, ++j) {
    InstallPlatformApp(GetProfile(0), i);
    InstallPlatformApp(verifier(), i);
  }

  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(AllProfilesHaveSameApps());
}

}  // namespace
