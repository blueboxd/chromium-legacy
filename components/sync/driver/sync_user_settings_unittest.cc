// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_user_settings_impl.h"

#include <memory>

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/driver/sync_service_crypto.h"
#include "components/sync/engine/configure_reason.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

ModelTypeSet GetPreferredUserTypes(
    const SyncUserSettingsImpl& sync_user_settings) {
  return Intersection(UserTypes(), sync_user_settings.GetPreferredDataTypes());
}

class SyncUserSettingsTest : public testing::Test {
 protected:
  SyncUserSettingsTest() {
    SyncPrefs::RegisterProfilePrefs(pref_service_.registry());
    sync_prefs_ = std::make_unique<SyncPrefs>(&pref_service_);

    sync_service_crypto_ = std::make_unique<SyncServiceCrypto>(
        /*notify_observers=*/base::DoNothing(),
        /*reconfigure=*/base::DoNothing(), sync_prefs_.get());
  }

  std::unique_ptr<SyncUserSettingsImpl> MakeSyncUserSettings(
      ModelTypeSet registered_types) {
    return std::make_unique<SyncUserSettingsImpl>(
        sync_service_crypto_.get(), sync_prefs_.get(),
        /*preference_provider=*/nullptr, registered_types,
        /*sync_allowed_by_platform_changed=*/
        base::DoNothing());
  }

  // The order of fields matters because it determines destruction order and
  // fields are dependent.
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  std::unique_ptr<SyncPrefs> sync_prefs_;
  std::unique_ptr<SyncServiceCrypto> sync_service_crypto_;
};

// TODO(crbug.com/950874): consider removing this test. The migration and the
// test itself are old and the test is full of workarounds to mimic old
// behavior, but the migration triggering logic was changed in
// crbug.com/906611.
TEST_F(SyncUserSettingsTest, DeleteDirectivesAndProxyTabsMigration) {
  // Simulate an upgrade to delete directives + proxy tabs support. None of the
  // new types or their pref group types should be registering, ensuring they
  // don't have pref values.
  ModelTypeSet registered_types = UserTypes();
  registered_types.Remove(PROXY_TABS);
  registered_types.Remove(TYPED_URLS);
  registered_types.Remove(SESSIONS);
  registered_types.Remove(HISTORY_DELETE_DIRECTIVES);

  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(registered_types);

  // Enable all other types.
  sync_user_settings->SetSelectedTypes(
      /*keep_everything_synced=*/false,
      /*selected_types=*/sync_user_settings->GetRegisteredSelectableTypes());

  // Manually enable typed urls (to simulate the old world) and perform the
  // migration to check it doesn't affect the proxy tab preference value.
  pref_service_.SetBoolean(prefs::kSyncTypedUrls, true);
  // TODO(crbug.com/906611): now we make an extra assumption that the migration
  // can be called a second time and it will do the real migration if during the
  // first call this migration wasn't needed. Maybe consider splitting this
  // test?
  MigrateSessionsToProxyTabsPrefs(&pref_service_);

  // Register all user types.
  sync_user_settings = MakeSyncUserSettings(UserTypes());
  // Proxy tabs should not be enabled (since sessions wasn't), but history
  // delete directives should (since typed urls was).
  ModelTypeSet preferred_types = sync_user_settings->GetPreferredDataTypes();
  EXPECT_FALSE(preferred_types.Has(PROXY_TABS));
  EXPECT_TRUE(preferred_types.Has(HISTORY_DELETE_DIRECTIVES));

  // Now manually enable sessions and perform the migration, which should result
  // in proxy tabs also being enabled. Also, manually disable typed urls, which
  // should mean that history delete directives are not enabled.
  pref_service_.SetBoolean(prefs::kSyncTypedUrls, false);
  pref_service_.SetBoolean(prefs::kSyncSessions, true);
  MigrateSessionsToProxyTabsPrefs(&pref_service_);

  preferred_types = sync_user_settings->GetPreferredDataTypes();
  EXPECT_TRUE(preferred_types.Has(PROXY_TABS));
  EXPECT_FALSE(preferred_types.Has(HISTORY_DELETE_DIRECTIVES));
}

TEST_F(SyncUserSettingsTest, PreferredTypesSyncEverything) {
  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(UserTypes());

  EXPECT_TRUE(sync_user_settings->IsSyncEverythingEnabled());
  EXPECT_EQ(UserTypes(), GetPreferredUserTypes(*sync_user_settings));
  for (UserSelectableType type : UserSelectableTypeSet::All()) {
    sync_user_settings->SetSelectedTypes(/*sync_everything=*/true,
                                         /*selected_type=*/{type});
    EXPECT_EQ(UserTypes(), GetPreferredUserTypes(*sync_user_settings));
  }
}

TEST_F(SyncUserSettingsTest, PreferredTypesNotKeepEverythingSynced) {
  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(UserTypes());

  sync_user_settings->SetSelectedTypes(
      /*sync_everything=*/false,
      /*selected_types=*/UserSelectableTypeSet());
  ASSERT_NE(UserTypes(), GetPreferredUserTypes(*sync_user_settings));
  for (UserSelectableType type : UserSelectableTypeSet::All()) {
    ModelTypeSet expected_preferred_types{
        UserSelectableTypeToCanonicalModelType(type)};
    if (type == UserSelectableType::kAutofill) {
      expected_preferred_types.Put(AUTOFILL_PROFILE);
      expected_preferred_types.Put(AUTOFILL_WALLET_DATA);
      expected_preferred_types.Put(AUTOFILL_WALLET_METADATA);
    }
    if (type == UserSelectableType::kPreferences) {
      expected_preferred_types.Put(DICTIONARY);
      expected_preferred_types.Put(PRIORITY_PREFERENCES);
      expected_preferred_types.Put(SEARCH_ENGINES);
    }
    if (type == UserSelectableType::kApps) {
      expected_preferred_types.Put(APP_LIST);
      expected_preferred_types.Put(APP_SETTINGS);
      expected_preferred_types.Put(ARC_PACKAGE);
      expected_preferred_types.Put(WEB_APPS);
    }
    if (type == UserSelectableType::kExtensions) {
      expected_preferred_types.Put(EXTENSION_SETTINGS);
    }
    if (type == UserSelectableType::kHistory) {
      expected_preferred_types.Put(HISTORY_DELETE_DIRECTIVES);
      expected_preferred_types.Put(SESSIONS);
      expected_preferred_types.Put(FAVICON_IMAGES);
      expected_preferred_types.Put(FAVICON_TRACKING);
      expected_preferred_types.Put(USER_EVENTS);
    }
    if (type == UserSelectableType::kTabs) {
      expected_preferred_types.Put(SESSIONS);
      expected_preferred_types.Put(FAVICON_IMAGES);
      expected_preferred_types.Put(FAVICON_TRACKING);
      expected_preferred_types.Put(SEND_TAB_TO_SELF);
    }

    expected_preferred_types.PutAll(AlwaysPreferredUserTypes());
    sync_user_settings->SetSelectedTypes(/*sync_everything=*/false,
                                         /*selected_types=*/{type});
    EXPECT_EQ(expected_preferred_types,
              GetPreferredUserTypes(*sync_user_settings));
  }
}

// Device info should always be enabled.
TEST_F(SyncUserSettingsTest, DeviceInfo) {
  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(UserTypes());
  EXPECT_TRUE(sync_user_settings->GetPreferredDataTypes().Has(DEVICE_INFO));
  sync_user_settings->SetSelectedTypes(
      /*keep_everything_synced=*/true,
      /*selected_types=*/UserSelectableTypeSet::All());
  EXPECT_TRUE(sync_user_settings->GetPreferredDataTypes().Has(DEVICE_INFO));
  sync_user_settings->SetSelectedTypes(
      /*keep_everything_synced=*/false,
      /*selected_types=*/UserSelectableTypeSet::All());
  EXPECT_TRUE(sync_user_settings->GetPreferredDataTypes().Has(DEVICE_INFO));
  sync_user_settings = MakeSyncUserSettings(ModelTypeSet(DEVICE_INFO));
  sync_user_settings->SetSelectedTypes(
      /*keep_everything_synced=*/false,
      /*selected_types=*/UserSelectableTypeSet());
  EXPECT_TRUE(sync_user_settings->GetPreferredDataTypes().Has(DEVICE_INFO));
}

// User Consents should always be enabled.
TEST_F(SyncUserSettingsTest, UserConsents) {
  std::unique_ptr<SyncUserSettingsImpl> sync_user_settings =
      MakeSyncUserSettings(UserTypes());
  EXPECT_TRUE(sync_user_settings->GetPreferredDataTypes().Has(USER_CONSENTS));
  sync_user_settings->SetSelectedTypes(
      /*keep_everything_synced=*/true,
      /*selected_types=*/UserSelectableTypeSet::All());
  EXPECT_TRUE(sync_user_settings->GetPreferredDataTypes().Has(USER_CONSENTS));
  sync_user_settings->SetSelectedTypes(
      /*keep_everything_synced=*/false,
      /*selected_types=*/UserSelectableTypeSet::All());
  EXPECT_TRUE(sync_user_settings->GetPreferredDataTypes().Has(USER_CONSENTS));
  sync_user_settings = MakeSyncUserSettings(ModelTypeSet(USER_CONSENTS));
  sync_user_settings->SetSelectedTypes(
      /*keep_everything_synced=*/false,
      /*selected_types=*/UserSelectableTypeSet());
  EXPECT_TRUE(sync_user_settings->GetPreferredDataTypes().Has(USER_CONSENTS));
}

}  // namespace

}  // namespace syncer
