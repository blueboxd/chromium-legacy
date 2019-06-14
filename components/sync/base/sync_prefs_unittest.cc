// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/sync_prefs.h"

#include <memory>

#include "base/command_line.h"
#include "base/test/scoped_task_environment.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/pref_value_store.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/base/pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using ::testing::InSequence;
using ::testing::StrictMock;

class SyncPrefsTest : public testing::Test {
 protected:
  SyncPrefsTest() {
    SyncPrefs::RegisterProfilePrefs(pref_service_.registry());
    sync_prefs_ = std::make_unique<SyncPrefs>(&pref_service_);
  }

  base::test::ScopedTaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  std::unique_ptr<SyncPrefs> sync_prefs_;
};

// Verify that invalidation versions are persisted and loaded correctly.
TEST_F(SyncPrefsTest, InvalidationVersions) {
  std::map<ModelType, int64_t> versions;
  versions[BOOKMARKS] = 10;
  versions[SESSIONS] = 20;
  versions[PREFERENCES] = 30;

  sync_prefs_->UpdateInvalidationVersions(versions);

  std::map<ModelType, int64_t> versions2;
  sync_prefs_->GetInvalidationVersions(&versions2);

  EXPECT_EQ(versions.size(), versions2.size());
  for (auto map_iter : versions2) {
    EXPECT_EQ(versions[map_iter.first], map_iter.second);
  }
}

TEST_F(SyncPrefsTest, PollInterval) {
  EXPECT_TRUE(sync_prefs_->GetPollInterval().is_zero());

  sync_prefs_->SetPollInterval(base::TimeDelta::FromMinutes(30));

  EXPECT_FALSE(sync_prefs_->GetPollInterval().is_zero());
  EXPECT_EQ(sync_prefs_->GetPollInterval().InMinutes(), 30);
}

class MockSyncPrefObserver : public SyncPrefObserver {
 public:
  MOCK_METHOD1(OnSyncManagedPrefChange, void(bool));
  MOCK_METHOD1(OnFirstSetupCompletePrefChange, void(bool));
  MOCK_METHOD1(OnSyncRequestedPrefChange, void(bool));
  MOCK_METHOD0(OnPreferredDataTypesPrefChange, void());
};

TEST_F(SyncPrefsTest, ObservedPrefs) {
  StrictMock<MockSyncPrefObserver> mock_sync_pref_observer;
  InSequence dummy;
  EXPECT_CALL(mock_sync_pref_observer, OnSyncManagedPrefChange(true));
  EXPECT_CALL(mock_sync_pref_observer, OnSyncManagedPrefChange(false));
  EXPECT_CALL(mock_sync_pref_observer, OnFirstSetupCompletePrefChange(true));
  EXPECT_CALL(mock_sync_pref_observer, OnFirstSetupCompletePrefChange(false));
  EXPECT_CALL(mock_sync_pref_observer, OnSyncRequestedPrefChange(false));
  EXPECT_CALL(mock_sync_pref_observer, OnSyncRequestedPrefChange(true));

  ASSERT_FALSE(sync_prefs_->IsManaged());
  ASSERT_FALSE(sync_prefs_->IsFirstSetupComplete());
  ASSERT_TRUE(sync_prefs_->IsSyncRequested());

  sync_prefs_->AddSyncPrefObserver(&mock_sync_pref_observer);

  sync_prefs_->SetManagedForTest(true);
  EXPECT_TRUE(sync_prefs_->IsManaged());
  sync_prefs_->SetManagedForTest(false);
  EXPECT_FALSE(sync_prefs_->IsManaged());

  sync_prefs_->SetFirstSetupComplete();
  EXPECT_TRUE(sync_prefs_->IsFirstSetupComplete());
  // There's no direct way to clear the first-setup-complete bit, so just reset
  // all prefs instead.
  sync_prefs_->ClearPreferences();
  EXPECT_FALSE(sync_prefs_->IsFirstSetupComplete());

  sync_prefs_->SetSyncRequested(false);
  EXPECT_FALSE(sync_prefs_->IsSyncRequested());
  sync_prefs_->SetSyncRequested(true);
  EXPECT_TRUE(sync_prefs_->IsSyncRequested());

  sync_prefs_->RemoveSyncPrefObserver(&mock_sync_pref_observer);
}

TEST_F(SyncPrefsTest, ClearPreferences) {
  EXPECT_FALSE(sync_prefs_->IsFirstSetupComplete());
  EXPECT_EQ(base::Time(), sync_prefs_->GetLastSyncedTime());
  EXPECT_TRUE(sync_prefs_->GetEncryptionBootstrapToken().empty());

  sync_prefs_->SetFirstSetupComplete();
  sync_prefs_->SetLastSyncedTime(base::Time::Now());
  sync_prefs_->SetEncryptionBootstrapToken("token");

  EXPECT_TRUE(sync_prefs_->IsFirstSetupComplete());
  EXPECT_NE(base::Time(), sync_prefs_->GetLastSyncedTime());
  EXPECT_EQ("token", sync_prefs_->GetEncryptionBootstrapToken());

  sync_prefs_->ClearPreferences();

  EXPECT_FALSE(sync_prefs_->IsFirstSetupComplete());
  EXPECT_EQ(base::Time(), sync_prefs_->GetLastSyncedTime());
  EXPECT_TRUE(sync_prefs_->GetEncryptionBootstrapToken().empty());
}

// -----------------------------------------------------------------------------
// Test that manipulate selected types.
// -----------------------------------------------------------------------------

TEST_F(SyncPrefsTest, Basic) {
  EXPECT_FALSE(sync_prefs_->IsFirstSetupComplete());
  sync_prefs_->SetFirstSetupComplete();
  EXPECT_TRUE(sync_prefs_->IsFirstSetupComplete());

  EXPECT_TRUE(sync_prefs_->IsSyncRequested());
  sync_prefs_->SetSyncRequested(false);
  EXPECT_FALSE(sync_prefs_->IsSyncRequested());
  sync_prefs_->SetSyncRequested(true);
  EXPECT_TRUE(sync_prefs_->IsSyncRequested());

  EXPECT_EQ(base::Time(), sync_prefs_->GetLastSyncedTime());
  const base::Time& now = base::Time::Now();
  sync_prefs_->SetLastSyncedTime(now);
  EXPECT_EQ(now, sync_prefs_->GetLastSyncedTime());

  EXPECT_TRUE(sync_prefs_->HasKeepEverythingSynced());
  sync_prefs_->SetSelectedTypes(
      /*keep_everything_synced=*/false,
      /*registered_types=*/UserSelectableTypeSet::All(),
      /*selected_types=*/UserSelectableTypeSet::All());
  EXPECT_FALSE(sync_prefs_->HasKeepEverythingSynced());
  sync_prefs_->SetSelectedTypes(
      /*keep_everything_synced=*/true,
      /*registered_types=*/UserSelectableTypeSet::All(),
      /*selected_types=*/UserSelectableTypeSet());
  EXPECT_TRUE(sync_prefs_->HasKeepEverythingSynced());

  EXPECT_TRUE(sync_prefs_->GetEncryptionBootstrapToken().empty());
  sync_prefs_->SetEncryptionBootstrapToken("token");
  EXPECT_EQ("token", sync_prefs_->GetEncryptionBootstrapToken());
}

TEST_F(SyncPrefsTest, SelectedTypesKeepEverythingSynced) {
  EXPECT_TRUE(sync_prefs_->HasKeepEverythingSynced());

  EXPECT_EQ(UserSelectableTypeSet::All(), sync_prefs_->GetSelectedTypes());
  for (UserSelectableType type : UserSelectableTypeSet::All()) {
    sync_prefs_->SetSelectedTypes(
        /*keep_everything_synced=*/true,
        /*registered_types=*/UserSelectableTypeSet::All(),
        /*selected_types=*/{type});
    EXPECT_EQ(UserSelectableTypeSet::All(), sync_prefs_->GetSelectedTypes());
  }
}

TEST_F(SyncPrefsTest, SelectedTypesNotKeepEverythingSynced) {
  sync_prefs_->SetSelectedTypes(
      /*keep_everything_synced=*/false,
      /*registered_types=*/UserSelectableTypeSet::All(),
      /*selected_types=*/UserSelectableTypeSet());

  ASSERT_NE(UserSelectableTypeSet::All(), sync_prefs_->GetSelectedTypes());
  for (UserSelectableType type : UserSelectableTypeSet::All()) {
    sync_prefs_->SetSelectedTypes(
        /*keep_everything_synced=*/false,
        /*registered_types=*/UserSelectableTypeSet::All(),
        /*selected_types=*/{type});
    EXPECT_EQ(UserSelectableTypeSet{type}, sync_prefs_->GetSelectedTypes());
  }
}

}  // namespace

}  // namespace syncer
