// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/observers/sync_disable_observer.h"

#include "base/observer_list.h"
#include "components/sync/driver/sync_token_status.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/unified_consent/pref_names.h"
#include "components/unified_consent/unified_consent_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ukm {

namespace {

class MockSyncService : public syncer::TestSyncService {
 public:
  MockSyncService() {
    SetTransportState(TransportState::INITIALIZING);
    SetLastCycleSnapshot(syncer::SyncCycleSnapshot());
  }
  ~MockSyncService() override { Shutdown(); }

  void SetStatus(bool has_passphrase, bool history_enabled, bool active) {
    SetTransportState(active ? TransportState::ACTIVE
                             : TransportState::INITIALIZING);
    SetIsUsingSecondaryPassphrase(has_passphrase);
    SetPreferredDataTypes(
        history_enabled
            ? syncer::ModelTypeSet(syncer::HISTORY_DELETE_DIRECTIVES)
            : syncer::ModelTypeSet());

    // It doesn't matter what exactly we set here, it's only relevant that the
    // SyncCycleSnapshot is initialized at all.
    SetLastCycleSnapshot(syncer::SyncCycleSnapshot(
        /*birthday=*/std::string(), /*bag_of_chips=*/std::string(),
        syncer::ModelNeutralState(), syncer::ProgressMarkerMap(), false, 0, 0,
        0, true, 0, base::Time::Now(), base::Time::Now(),
        std::vector<int>(syncer::ModelType::NUM_ENTRIES, 0),
        std::vector<int>(syncer::ModelType::NUM_ENTRIES, 0),
        sync_pb::SyncEnums::UNKNOWN_ORIGIN, base::TimeDelta::FromMinutes(1),
        false));

    NotifyObserversOfStateChanged();
  }

  void SetAuthError(GoogleServiceAuthError::State error_state) {
    syncer::TestSyncService::SetAuthError(GoogleServiceAuthError(error_state));
    NotifyObserversOfStateChanged();
  }

  void Shutdown() override {
    for (auto& observer : observers_) {
      observer.OnSyncShutdown(this);
    }
  }

 private:
  // syncer::TestSyncService:
  void AddObserver(syncer::SyncServiceObserver* observer) override {
    observers_.AddObserver(observer);
  }
  void RemoveObserver(syncer::SyncServiceObserver* observer) override {
    observers_.RemoveObserver(observer);
  }

  void NotifyObserversOfStateChanged() {
    for (auto& observer : observers_) {
      observer.OnStateChanged(this);
    }
  }

  // The list of observers of the SyncService state.
  base::ObserverList<syncer::SyncServiceObserver>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(MockSyncService);
};

class TestSyncDisableObserver : public SyncDisableObserver {
 public:
  TestSyncDisableObserver() : purged_(false), notified_(false) {}
  ~TestSyncDisableObserver() override {}

  bool ResetPurged() {
    bool was_purged = purged_;
    purged_ = false;
    return was_purged;
  }

  bool ResetNotified() {
    bool notified = notified_;
    notified_ = false;
    return notified;
  }

 private:
  // SyncDisableObserver:
  void OnSyncPrefsChanged(bool must_purge) override {
    notified_ = true;
    purged_ = purged_ || must_purge;
  }
  bool purged_;
  bool notified_;
  DISALLOW_COPY_AND_ASSIGN(TestSyncDisableObserver);
};

class SyncDisableObserverTest : public testing::Test {
 public:
  SyncDisableObserverTest() {}
  void RegisterUrlKeyedAnonymizedDataCollectionPref(
      sync_preferences::TestingPrefServiceSyncable& prefs) {
    unified_consent::UnifiedConsentService::RegisterPrefs(prefs.registry());
  }

  void SetUrlKeyedAnonymizedDataCollectionEnabled(PrefService* prefs,
                                                  bool enabled) {
    prefs->SetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
        enabled);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncDisableObserverTest);
};

}  // namespace

TEST_F(SyncDisableObserverTest, NoProfiles) {
  TestSyncDisableObserver observer;
  EXPECT_FALSE(observer.SyncStateAllowsUkm());
  EXPECT_FALSE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
}

TEST_F(SyncDisableObserverTest, NotActive) {
  MockSyncService sync;
  sync.SetStatus(false, true, false);
  sync_preferences::TestingPrefServiceSyncable prefs;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs);
  TestSyncDisableObserver observer;
  observer.ObserveServiceForSyncDisables(&sync, &prefs);
  EXPECT_FALSE(observer.SyncStateAllowsUkm());
  EXPECT_FALSE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
}

TEST_F(SyncDisableObserverTest, OneEnabled) {
  sync_preferences::TestingPrefServiceSyncable prefs;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs);
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs, true);
  MockSyncService sync;
  for (bool has_passphrase : {true, false}) {
    for (bool history_enabled : {true, false}) {
      TestSyncDisableObserver observer;
      sync.SetStatus(has_passphrase, history_enabled, /*active=*/true);
      observer.ObserveServiceForSyncDisables(&sync, &prefs);
      EXPECT_TRUE(observer.SyncStateAllowsUkm());
      EXPECT_TRUE(observer.ResetNotified());
      EXPECT_FALSE(observer.ResetPurged());
    }
  }
}

TEST_F(SyncDisableObserverTest, MixedProfiles) {
  sync_preferences::TestingPrefServiceSyncable prefs1;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs1);
  sync_preferences::TestingPrefServiceSyncable prefs2;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs2);

  TestSyncDisableObserver observer;
  MockSyncService sync1;
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs1, false);
  observer.ObserveServiceForSyncDisables(&sync1, &prefs1);
  MockSyncService sync2;
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs2, true);
  observer.ObserveServiceForSyncDisables(&sync2, &prefs2);
  EXPECT_FALSE(observer.SyncStateAllowsUkm());
  EXPECT_FALSE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
}

TEST_F(SyncDisableObserverTest, TwoEnabled) {
  sync_preferences::TestingPrefServiceSyncable prefs1;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs1);
  sync_preferences::TestingPrefServiceSyncable prefs2;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs2);

  TestSyncDisableObserver observer;
  MockSyncService sync1;
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs1, true);
  observer.ObserveServiceForSyncDisables(&sync1, &prefs1);
  EXPECT_TRUE(observer.ResetNotified());
  MockSyncService sync2;
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs2, true);
  observer.ObserveServiceForSyncDisables(&sync2, &prefs2);
  EXPECT_TRUE(observer.SyncStateAllowsUkm());
  EXPECT_FALSE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
}

TEST_F(SyncDisableObserverTest, OneAddRemove) {
  sync_preferences::TestingPrefServiceSyncable prefs;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs);
  TestSyncDisableObserver observer;
  MockSyncService sync;
  observer.ObserveServiceForSyncDisables(&sync, &prefs);
  EXPECT_FALSE(observer.SyncStateAllowsUkm());
  EXPECT_FALSE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs, true);
  EXPECT_TRUE(observer.SyncStateAllowsUkm());
  EXPECT_TRUE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
  sync.Shutdown();
  EXPECT_FALSE(observer.SyncStateAllowsUkm());
  EXPECT_TRUE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
}

TEST_F(SyncDisableObserverTest, PurgeOnDisable) {
  sync_preferences::TestingPrefServiceSyncable prefs;
  RegisterUrlKeyedAnonymizedDataCollectionPref(prefs);
  TestSyncDisableObserver observer;
  MockSyncService sync;
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs, true);
  observer.ObserveServiceForSyncDisables(&sync, &prefs);
  EXPECT_TRUE(observer.SyncStateAllowsUkm());
  EXPECT_TRUE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
  SetUrlKeyedAnonymizedDataCollectionEnabled(&prefs, false);
  EXPECT_FALSE(observer.SyncStateAllowsUkm());
  EXPECT_TRUE(observer.ResetNotified());
  EXPECT_TRUE(observer.ResetPurged());
  sync.Shutdown();
  EXPECT_FALSE(observer.SyncStateAllowsUkm());
  EXPECT_FALSE(observer.ResetNotified());
  EXPECT_FALSE(observer.ResetPurged());
}

}  // namespace ukm
