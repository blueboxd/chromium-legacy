// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/sync/test/integration/feature_toggler.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/driver/sync_driver_switches.h"

namespace {

using passwords_helper::AddLogin;
using passwords_helper::CreateTestPasswordForm;
using passwords_helper::GetPasswordCount;
using passwords_helper::GetPasswordStore;
using passwords_helper::GetVerifierPasswordCount;
using passwords_helper::GetVerifierPasswordStore;
using passwords_helper::ProfileContainsSamePasswordFormsAsVerifier;

using autofill::PasswordForm;

using testing::ElementsAre;
using testing::IsEmpty;

class SingleClientPasswordsSyncTest : public FeatureToggler, public SyncTest {
 public:
  SingleClientPasswordsSyncTest()
      : FeatureToggler(switches::kSyncUSSPasswords), SyncTest(SINGLE_CLIENT) {}
  ~SingleClientPasswordsSyncTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientPasswordsSyncTest);
};

IN_PROC_BROWSER_TEST_P(SingleClientPasswordsSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  PasswordForm form = CreateTestPasswordForm(0);
  AddLogin(GetVerifierPasswordStore(), form);
  ASSERT_EQ(1, GetVerifierPasswordCount());
  AddLogin(GetPasswordStore(0), form);
  ASSERT_EQ(1, GetPasswordCount(0));

  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(ProfileContainsSamePasswordFormsAsVerifier(0));
  ASSERT_EQ(1, GetPasswordCount(0));
}

// Verifies that committed passwords contain the appropriate proto fields, and
// in particular lack some others that could potentially contain unencrypted
// data. In this test, custom passphrase is NOT set.
IN_PROC_BROWSER_TEST_P(SingleClientPasswordsSyncTest,
                       CommitWithoutCustomPassphrase) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  PasswordForm form = CreateTestPasswordForm(0);
  AddLogin(GetVerifierPasswordStore(), form);
  ASSERT_EQ(1, GetVerifierPasswordCount());
  AddLogin(GetPasswordStore(0), form);
  ASSERT_EQ(1, GetPasswordCount(0));
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  const std::vector<sync_pb::SyncEntity> entities =
      fake_server_->GetSyncEntitiesByModelType(syncer::PASSWORDS);
  ASSERT_EQ(1U, entities.size());
  EXPECT_EQ("", entities[0].non_unique_name());
  EXPECT_TRUE(entities[0].specifics().password().has_encrypted());
  EXPECT_FALSE(
      entities[0].specifics().password().has_client_only_encrypted_data());
  EXPECT_TRUE(entities[0].specifics().password().has_unencrypted_metadata());
  EXPECT_TRUE(
      entities[0].specifics().password().unencrypted_metadata().has_url());
}

// Same as above but with custom passphrase set, which requires to prune commit
// data even further.
IN_PROC_BROWSER_TEST_P(SingleClientPasswordsSyncTest,
                       CommitWithCustomPassphrase) {
  SetEncryptionPassphraseForClient(/*index=*/0, "hunter2");
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  PasswordForm form = CreateTestPasswordForm(0);
  AddLogin(GetVerifierPasswordStore(), form);
  ASSERT_EQ(1, GetVerifierPasswordCount());
  AddLogin(GetPasswordStore(0), form);
  ASSERT_EQ(1, GetPasswordCount(0));
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  const std::vector<sync_pb::SyncEntity> entities =
      fake_server_->GetSyncEntitiesByModelType(syncer::PASSWORDS);
  ASSERT_EQ(1U, entities.size());
  EXPECT_EQ("", entities[0].non_unique_name());
  EXPECT_TRUE(entities[0].specifics().password().has_encrypted());
  EXPECT_FALSE(
      entities[0].specifics().password().has_client_only_encrypted_data());
  EXPECT_FALSE(entities[0].specifics().password().has_unencrypted_metadata());
}

// Tests the scenario when a syncing user enables a custom passphrase. PASSWORDS
// should be recommitted with the new encryption key.
IN_PROC_BROWSER_TEST_P(SingleClientPasswordsSyncTest,
                       ReencryptsDataWhenPassphraseIsSet) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(ServerNigoriChecker(GetSyncService(0), fake_server_.get(),
                                  syncer::PassphraseType::KEYSTORE_PASSPHRASE)
                  .Wait());

  PasswordForm form = CreateTestPasswordForm(0);
  AddLogin(GetVerifierPasswordStore(), form);
  ASSERT_EQ(1, GetVerifierPasswordCount());
  AddLogin(GetPasswordStore(0), form);
  ASSERT_EQ(1, GetPasswordCount(0));
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  std::string prior_encryption_key_name;
  {
    const std::vector<sync_pb::SyncEntity> entities =
        fake_server_->GetSyncEntitiesByModelType(syncer::PASSWORDS);
    ASSERT_EQ(1U, entities.size());
    ASSERT_EQ("", entities[0].non_unique_name());
    ASSERT_TRUE(entities[0].specifics().password().has_encrypted());
    ASSERT_FALSE(
        entities[0].specifics().password().has_client_only_encrypted_data());
    ASSERT_TRUE(entities[0].specifics().password().has_unencrypted_metadata());
    prior_encryption_key_name =
        entities[0].specifics().password().encrypted().key_name();
  }

  ASSERT_FALSE(prior_encryption_key_name.empty());

  GetSyncService(0)->GetUserSettings()->SetEncryptionPassphrase("hunter2");
  ASSERT_TRUE(ServerNigoriChecker(GetSyncService(0), fake_server_.get(),
                                  syncer::PassphraseType::CUSTOM_PASSPHRASE)
                  .Wait());
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  const std::vector<sync_pb::SyncEntity> entities =
      fake_server_->GetSyncEntitiesByModelType(syncer::PASSWORDS);
  ASSERT_EQ(1U, entities.size());
  EXPECT_EQ("", entities[0].non_unique_name());
  EXPECT_TRUE(entities[0].specifics().password().has_encrypted());
  EXPECT_FALSE(
      entities[0].specifics().password().has_client_only_encrypted_data());
  EXPECT_FALSE(entities[0].specifics().password().has_unencrypted_metadata());

  const std::string new_encryption_key_name =
      entities[0].specifics().password().encrypted().key_name();
  EXPECT_FALSE(new_encryption_key_name.empty());
  EXPECT_NE(new_encryption_key_name, prior_encryption_key_name);
}

IN_PROC_BROWSER_TEST_P(SingleClientPasswordsSyncTest,
                       PRE_PersistProgressMarkerOnRestart) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  PasswordForm form = CreateTestPasswordForm(0);
  AddLogin(GetPasswordStore(0), form);
  ASSERT_EQ(1, GetPasswordCount(0));
  // Setup sync, wait for its completion, and make sure changes were synced.
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  // Upon a local creation, the received update will be seen as reflection and
  // get counted as incremental update.
  EXPECT_EQ(
      1, histogram_tester.GetBucketCount("Sync.ModelTypeEntityChange3.PASSWORD",
                                         /*REMOTE_NON_INITIAL_UPDATE=*/4));
}

IN_PROC_BROWSER_TEST_P(SingleClientPasswordsSyncTest,
                       PersistProgressMarkerOnRestart) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  ASSERT_EQ(1, GetPasswordCount(0));
#if defined(CHROMEOS)
  // signin::SetRefreshTokenForPrimaryAccount() is needed on ChromeOS in order
  // to get a non-empty refresh token on startup.
  GetClient(0)->SignInPrimaryAccount();
#endif  // defined(CHROMEOS)
  ASSERT_TRUE(GetClient(0)->AwaitEngineInitialization());

  // After restart, the last sync cycle snapshot should be empty. Once a sync
  // request happened (e.g. by a poll), that snapshot is populated. We use the
  // following checker to simply wait for an non-empty snapshot.
  EXPECT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  // If that metadata hasn't been properly persisted, the password stored on the
  // server will be received at the client as an initial update or an
  // incremental once.
  EXPECT_EQ(
      0, histogram_tester.GetBucketCount("Sync.ModelTypeEntityChange3.PASSWORD",
                                         /*REMOTE_INITIAL_UPDATE=*/5));
  EXPECT_EQ(
      0, histogram_tester.GetBucketCount("Sync.ModelTypeEntityChange3.PASSWORD",
                                         /*REMOTE_NON_INITIAL_UPDATE=*/4));
}

INSTANTIATE_TEST_SUITE_P(USS,
                         SingleClientPasswordsSyncTest,
                         ::testing::Values(false, true));

class SingleClientPasswordsSyncUssMigratorTest : public SyncTest {
 public:
  SingleClientPasswordsSyncUssMigratorTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientPasswordsSyncUssMigratorTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientPasswordsSyncUssMigratorTest);
};

// Creates and syncs two passwords before USS being enabled.
IN_PROC_BROWSER_TEST_F(SingleClientPasswordsSyncUssMigratorTest,
                       PRE_ExerciseUssMigrator) {
  base::test::ScopedFeatureList override_features;
  override_features.InitAndDisableFeature(switches::kSyncUSSPasswords);

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  AddLogin(GetPasswordStore(0), CreateTestPasswordForm(0));
  AddLogin(GetPasswordStore(0), CreateTestPasswordForm(1));
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_EQ(2, GetPasswordCount(0));
}

// TODO(https://crbug.com/952074): re-enable once flakiness is addressed.
#if defined(THREAD_SANITIZER)
#define MAYBE_ExerciseUssMigrator DISABLED_ExerciseUssMigrator
#else
#define MAYBE_ExerciseUssMigrator ExerciseUssMigrator
#endif

// Now that local passwords, the local sync directory and the sever are
// populated with two passwords, USS is enabled for passwords.
IN_PROC_BROWSER_TEST_F(SingleClientPasswordsSyncUssMigratorTest,
                       MAYBE_ExerciseUssMigrator) {
  base::test::ScopedFeatureList override_features;
  override_features.InitAndEnableFeature(switches::kSyncUSSPasswords);

  base::HistogramTester histogram_tester;
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  ASSERT_EQ(2, GetPasswordCount(0));
#if defined(CHROMEOS)
  // signin::SetRefreshTokenForPrimaryAccount() is needed on ChromeOS in order
  // to get a non-empty refresh token on startup.
  GetClient(0)->SignInPrimaryAccount();
#endif  // defined(CHROMEOS)
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());
  ASSERT_EQ(2, GetPasswordCount(0));

  EXPECT_EQ(1, histogram_tester.GetBucketCount(
                   "Sync.USSMigrationSuccess",
                   syncer::ModelTypeToHistogramInt(syncer::PASSWORDS)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Sync.USSMigrationEntityCount.PASSWORD"),
      ElementsAre(base::Bucket(/*min=*/2, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples("Sync.DataTypeStartFailures2"),
              IsEmpty());
  EXPECT_EQ(
      0, histogram_tester.GetBucketCount("Sync.ModelTypeEntityChange3.PASSWORD",
                                         /*REMOTE_INITIAL_UPDATE=*/5));
}

}  // namespace
