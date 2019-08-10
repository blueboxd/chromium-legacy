// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <limits>
#include <tuple>

#include "base/guid.h"
#include "base/hash/hash.h"
#include "base/macros.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/engine/model_safe_worker.h"

using passwords_helper::AddLogin;
using passwords_helper::AllProfilesContainSamePasswordForms;
using passwords_helper::AllProfilesContainSamePasswordFormsAsVerifier;
using passwords_helper::CreateTestPasswordForm;
using passwords_helper::GetLogins;
using passwords_helper::GetPasswordCount;
using passwords_helper::GetPasswordStore;
using passwords_helper::GetVerifierPasswordCount;
using passwords_helper::GetVerifierPasswordStore;
using passwords_helper::RemoveLogin;
using passwords_helper::RemoveLogins;
using passwords_helper::UpdateLogin;

using autofill::PasswordForm;

static const char* kValidPassphrase = "passphrase!";

class TwoClientPasswordsSyncTest
    : public testing::WithParamInterface<std::tuple<bool, bool>>,
      public SyncTest {
 public:
  TwoClientPasswordsSyncTest() : SyncTest(TWO_CLIENT) {}

  ~TwoClientPasswordsSyncTest() override {}

 protected:
  // TODO(crbug.com/915219): This leads to a data race and thus all tests here
  // are disabled on TSan. It is hard to avoid as overriding g_feature_list
  // after it has been used is needed for this test (by setting up each client
  // with a different ScopedFeatureList).
  void BeforeSetupClient(int index) override {
    const bool should_enable_uss =
        index == 0 ? std::get<0>(GetParam()) : std::get<1>(GetParam());

    // In order to avoid test flakiness, for any client other than the first, we
    // need to make sure the feature toggle has been fully read by PasswordStore
    // before overriding it again. The way to achieve that, for the current
    // implementation of PasswordStore, is to make a round trip to the backend
    // sequence, which guarantees that initialization has completed.
    if (index != 0) {
      // We ignore the returned value since all we want is to wait for the
      // round trip to be completed.
      GetPasswordCount(index - 1);
    }

    // The value of the feature kSyncUSSPasswords only matters during the
    // setup of each client, when the profile is created, ProfileSyncService
    // instantiated as well as the datatype controllers. By overriding the
    // feature, we can influence whether client |index| is running with the new
    // codepath or the legacy one.
    override_features_ = std::make_unique<base::test::ScopedFeatureList>();
    if (should_enable_uss) {
      override_features_->InitAndEnableFeature(switches::kSyncUSSPasswords);
    } else {
      override_features_->InitAndDisableFeature(switches::kSyncUSSPasswords);
    }
  }

 private:
  std::unique_ptr<base::test::ScopedFeatureList> override_features_;

  DISALLOW_COPY_AND_ASSIGN(TwoClientPasswordsSyncTest);
};

// Flaky on TSAN: crbug.com/915219
#if defined(THREAD_SANITIZER)
#define MAYBE_Add DISABLED_Add
#else
#define MAYBE_Add Add
#endif
IN_PROC_BROWSER_TEST_P(TwoClientPasswordsSyncTest, E2E_ENABLED(MAYBE_Add)) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(SamePasswordFormsChecker().Wait());

  PasswordForm form = CreateTestPasswordForm(0);
  AddLogin(GetPasswordStore(0), form);
  ASSERT_EQ(1, GetPasswordCount(0));

  ASSERT_TRUE(SamePasswordFormsChecker().Wait());
  ASSERT_EQ(1, GetPasswordCount(1));
}

// Flaky on TSAN: crbug.com/915219
#if defined(THREAD_SANITIZER)
#define MAYBE_Race DISABLED_Race
#else
#define MAYBE_Race Race
#endif
IN_PROC_BROWSER_TEST_P(TwoClientPasswordsSyncTest, E2E_ENABLED(MAYBE_Race)) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordForms());

  PasswordForm form0 = CreateTestPasswordForm(0);
  AddLogin(GetPasswordStore(0), form0);

  PasswordForm form1 = form0;
  form1.password_value = base::ASCIIToUTF16("new_password");
  AddLogin(GetPasswordStore(1), form1);

  ASSERT_TRUE(SamePasswordFormsChecker().Wait());
}

// Flaky on TSAN: crbug.com/915219
#if defined(THREAD_SANITIZER)
#define MAYBE_MergeWithTheMostRecent DISABLED_MergeWithTheMostRecent
#else
#define MAYBE_MergeWithTheMostRecent MergeWithTheMostRecent
#endif
IN_PROC_BROWSER_TEST_P(TwoClientPasswordsSyncTest,
                       MAYBE_MergeWithTheMostRecent) {
  // Setup the test to have Form 0 and Form 1 added on both clients. Form 0 is
  // more recent on Client 0, and Form 1 is more recent on Client 1. They should
  // be merged such that recent passwords are chosen.

  base::Time now = base::Time::Now();
  base::Time yesterday = now - base::TimeDelta::FromDays(1);

  PasswordForm form0_recent = CreateTestPasswordForm(0);
  form0_recent.date_created = now;
  PasswordForm form0_old = CreateTestPasswordForm(0);
  form0_old.date_created = yesterday;

  PasswordForm form1_recent = CreateTestPasswordForm(1);
  form1_recent.date_created = now;
  PasswordForm form1_old = CreateTestPasswordForm(1);
  form1_old.date_created = yesterday;

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Add the passwords to Client 0.
  AddLogin(GetPasswordStore(0), form0_recent);
  AddLogin(GetPasswordStore(0), form1_old);
  // Enable sync on Client 0 and wait until they are committed.
  ASSERT_TRUE(GetClient(0)->SetupSync()) << "GetClient(0)->SetupSync() failed.";
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  // Add the passwords to Client 1.
  AddLogin(GetPasswordStore(1), form0_old);
  AddLogin(GetPasswordStore(1), form1_recent);

  // Enable sync on Client 1 and wait until all passwords are merged.
  ASSERT_TRUE(GetClient(1)->SetupSync()) << "GetClient(1)->SetupSync() failed.";
  ASSERT_TRUE(SamePasswordFormsChecker().Wait());

  // There should be only 2 passwords.
  EXPECT_EQ(2, GetPasswordCount(0));
  // All passwords should be the recent ones.
  for (const std::unique_ptr<PasswordForm>& form :
       GetLogins(GetPasswordStore(0))) {
    EXPECT_EQ(now, form->date_created);
  }
}

// Flaky on TSAN: crbug.com/915219
#if defined(THREAD_SANITIZER)
#define MAYBE_SetPassphraseAndAddPassword DISABLED_SetPassphraseAndAddPassword
#else
#define MAYBE_SetPassphraseAndAddPassword SetPassphraseAndAddPassword
#endif
IN_PROC_BROWSER_TEST_P(TwoClientPasswordsSyncTest,
                       E2E_ENABLED(MAYBE_SetPassphraseAndAddPassword)) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  GetSyncService(0)->GetUserSettings()->SetEncryptionPassphrase(
      kValidPassphrase);
  ASSERT_TRUE(PassphraseAcceptedChecker(GetSyncService(0)).Wait());

  ASSERT_TRUE(PassphraseRequiredChecker(GetSyncService(1)).Wait());
  ASSERT_TRUE(GetSyncService(1)->GetUserSettings()->SetDecryptionPassphrase(
      kValidPassphrase));
  ASSERT_TRUE(PassphraseAcceptedChecker(GetSyncService(1)).Wait());

  PasswordForm form = CreateTestPasswordForm(0);
  AddLogin(GetPasswordStore(0), form);
  ASSERT_EQ(1, GetPasswordCount(0));

  ASSERT_TRUE(SamePasswordFormsChecker().Wait());
}

// Flaky on TSAN: crbug.com/915219
#if defined(THREAD_SANITIZER)
#define MAYBE_Update DISABLED_Update
#else
#define MAYBE_Update Update
#endif
IN_PROC_BROWSER_TEST_P(TwoClientPasswordsSyncTest, MAYBE_Update) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());

  PasswordForm form = CreateTestPasswordForm(0);
  AddLogin(GetVerifierPasswordStore(), form);
  AddLogin(GetPasswordStore(0), form);

  // Wait for client 0 to commit and client 1 to receive the update.
  ASSERT_TRUE(SamePasswordFormsAsVerifierChecker(1).Wait());

  form.password_value = base::ASCIIToUTF16("new_password");
  UpdateLogin(GetVerifierPasswordStore(), form);
  UpdateLogin(GetPasswordStore(1), form);
  ASSERT_EQ(1, GetVerifierPasswordCount());

  // Wait for client 1 to commit and client 0 to receive the update.
  ASSERT_TRUE(SamePasswordFormsAsVerifierChecker(0).Wait());
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());
}

// Flaky on TSAN: crbug.com/915219
#if defined(THREAD_SANITIZER)
#define MAYBE_AddTwice DISABLED_AddTwice
#else
#define MAYBE_AddTwice AddTwice
#endif
IN_PROC_BROWSER_TEST_P(TwoClientPasswordsSyncTest, MAYBE_AddTwice) {
  // Password store supports adding the same form twice, so this is testing this
  // behaviour.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordForms());

  PasswordForm form = CreateTestPasswordForm(0);
  AddLogin(GetPasswordStore(0), form);
  ASSERT_EQ(1, GetPasswordCount(0));

  // Wait for client 0 to commit and client 1 to receive the update.
  ASSERT_TRUE(SamePasswordFormsChecker().Wait());
  ASSERT_EQ(1, GetPasswordCount(1));

  // Update the password and add it again to client 0.
  form.password_value = base::ASCIIToUTF16("new_password");
  AddLogin(GetPasswordStore(0), form);
  ASSERT_EQ(1, GetPasswordCount(0));

  // Wait for client 1 to receive the update.
  ASSERT_TRUE(SamePasswordFormsChecker().Wait());
  ASSERT_EQ(1, GetPasswordCount(1));
}

// Flaky on TSAN: crbug.com/915219
#if defined(THREAD_SANITIZER)
#define MAYBE_Delete DISABLED_Delete
#else
#define MAYBE_Delete Delete
#endif
IN_PROC_BROWSER_TEST_P(TwoClientPasswordsSyncTest, MAYBE_Delete) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());

  PasswordForm form0 = CreateTestPasswordForm(0);
  AddLogin(GetVerifierPasswordStore(), form0);
  AddLogin(GetPasswordStore(0), form0);
  PasswordForm form1 = CreateTestPasswordForm(1);
  AddLogin(GetVerifierPasswordStore(), form1);
  AddLogin(GetPasswordStore(0), form1);

  // Wait for client 0 to commit and client 1 to receive the update.
  ASSERT_TRUE(SamePasswordFormsAsVerifierChecker(1).Wait());

  RemoveLogin(GetPasswordStore(1), form0);
  RemoveLogin(GetVerifierPasswordStore(), form0);
  ASSERT_EQ(1, GetVerifierPasswordCount());

  // Wait for deletion from client 1 to propagate.
  ASSERT_TRUE(SamePasswordFormsAsVerifierChecker(0).Wait());
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());
}

#if defined(THREAD_SANITIZER)
// Flaky on TSAN: crbug.com/915219
#define MAYBE_SetPassphraseAndThenSetupSync \
  DISABLED_SetPassphraseAndThenSetupSync
#else
#define MAYBE_SetPassphraseAndThenSetupSync SetPassphraseAndThenSetupSync
#endif
IN_PROC_BROWSER_TEST_P(TwoClientPasswordsSyncTest,
                       MAYBE_SetPassphraseAndThenSetupSync) {
  ASSERT_TRUE(SetupClients());

  ASSERT_TRUE(GetClient(0)->SetupSync());
  GetSyncService(0)->GetUserSettings()->SetEncryptionPassphrase(
      kValidPassphrase);
  ASSERT_TRUE(PassphraseAcceptedChecker(GetSyncService(0)).Wait());
  // Wait for the client to commit the updates.
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  // When client 1 hits a passphrase required state, we can infer that
  // client 0's passphrase has been committed. to the server.
  ASSERT_TRUE(GetClient(1)->SetupSyncNoWaitForCompletion(
      GetRegisteredSelectableTypes(1)));
  ASSERT_TRUE(PassphraseRequiredChecker(GetSyncService(1)).Wait());

  // Get client 1 out of the passphrase required state.
  ASSERT_TRUE(GetSyncService(1)->GetUserSettings()->SetDecryptionPassphrase(
      kValidPassphrase));
  ASSERT_TRUE(PassphraseAcceptedChecker(GetSyncService(1)).Wait());

  // We must mark the setup complete now, since we just entered the passphrase
  // and the previous SetupSync() call failed.
  GetClient(1)->FinishSyncSetup();

  // Move around some passwords to make sure it's all working.
  PasswordForm form0 = CreateTestPasswordForm(0);
  AddLogin(GetPasswordStore(0), form0);

  ASSERT_TRUE(SamePasswordFormsChecker().Wait());
}

// Flaky on TSAN: crbug.com/915219
#if defined(THREAD_SANITIZER)
#define MAYBE_DeleteTwo DISABLED_DeleteTwo
#else
#define MAYBE_DeleteTwo DeleteTwo
#endif
IN_PROC_BROWSER_TEST_P(TwoClientPasswordsSyncTest, E2E_ONLY(MAYBE_DeleteTwo)) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordForms());

  PasswordForm form0 = CreateTestPasswordForm(base::Hash(base::GenerateGUID()));
  PasswordForm form1 = CreateTestPasswordForm(base::Hash(base::GenerateGUID()));
  AddLogin(GetPasswordStore(0), form0);
  AddLogin(GetPasswordStore(0), form1);

  const int init_password_count = GetPasswordCount(0);

  // Wait for client 0 to commit and client 1 to receive the update.
  ASSERT_TRUE(SamePasswordFormsChecker().Wait());
  ASSERT_EQ(init_password_count, GetPasswordCount(1));

  RemoveLogin(GetPasswordStore(1), form0);

  // Wait for deletion from client 1 to propagate.
  ASSERT_TRUE(SamePasswordFormsChecker().Wait());
  ASSERT_EQ(init_password_count - 1, GetPasswordCount(0));

  RemoveLogin(GetPasswordStore(1), form1);

  // Wait for deletion from client 1 to propagate.
  ASSERT_TRUE(SamePasswordFormsChecker().Wait());
  ASSERT_EQ(init_password_count - 2, GetPasswordCount(0));
}

// Flaky on TSAN: crbug.com/915219
#if defined(THREAD_SANITIZER)
#define MAYBE_DeleteAll DISABLED_DeleteAll
#else
#define MAYBE_DeleteAll DeleteAll
#endif
IN_PROC_BROWSER_TEST_P(TwoClientPasswordsSyncTest, MAYBE_DeleteAll) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());

  PasswordForm form0 = CreateTestPasswordForm(0);
  AddLogin(GetVerifierPasswordStore(), form0);
  AddLogin(GetPasswordStore(0), form0);
  PasswordForm form1 = CreateTestPasswordForm(1);
  AddLogin(GetVerifierPasswordStore(), form1);
  AddLogin(GetPasswordStore(0), form1);
  ASSERT_TRUE(SamePasswordFormsAsVerifierChecker(1).Wait());
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());

  RemoveLogins(GetPasswordStore(1));
  RemoveLogins(GetVerifierPasswordStore());
  ASSERT_TRUE(SamePasswordFormsAsVerifierChecker(0).Wait());
  ASSERT_TRUE(AllProfilesContainSamePasswordFormsAsVerifier());
  ASSERT_EQ(0, GetVerifierPasswordCount());
}

// Flaky on TSAN: crbug.com/915219
#if defined(THREAD_SANITIZER)
#define MAYBE_Merge DISABLED_Merge
#else
#define MAYBE_Merge Merge
#endif
IN_PROC_BROWSER_TEST_P(TwoClientPasswordsSyncTest, E2E_ENABLED(MAYBE_Merge)) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllProfilesContainSamePasswordForms());

  PasswordForm form0 = CreateTestPasswordForm(0);
  AddLogin(GetPasswordStore(0), form0);
  PasswordForm form1 = CreateTestPasswordForm(1);
  AddLogin(GetPasswordStore(1), form1);
  PasswordForm form2 = CreateTestPasswordForm(2);
  AddLogin(GetPasswordStore(1), form2);

  ASSERT_TRUE(SamePasswordFormsChecker().Wait());
  ASSERT_EQ(3, GetPasswordCount(0));
}

// Flaky on TSAN: crbug.com/915219
#if defined(THREAD_SANITIZER)
#define MAYBE_TwoClientAddPass DISABLED_TwoClientAddPass
#else
#define MAYBE_TwoClientAddPass TwoClientAddPass
#endif
IN_PROC_BROWSER_TEST_P(TwoClientPasswordsSyncTest,
                       E2E_ONLY(MAYBE_TwoClientAddPass)) {
  ASSERT_TRUE(SetupSync()) <<  "SetupSync() failed.";
  // All profiles should sync same passwords.
  ASSERT_TRUE(SamePasswordFormsChecker().Wait())
      << "Initial password forms did not match for all profiles";
  const int init_password_count = GetPasswordCount(0);

  // Add one new password per profile. A unique form is created for each to
  // prevent them from overwriting each other.
  for (int i = 0; i < num_clients(); ++i) {
    AddLogin(GetPasswordStore(i), CreateTestPasswordForm(base::RandInt(
                                      0, std::numeric_limits<int32_t>::max())));
  }

  // Blocks and waits for password forms in all profiles to match.
  ASSERT_TRUE(SamePasswordFormsChecker().Wait());

  // Check that total number of passwords is as expected.
  for (int i = 0; i < num_clients(); ++i) {
    ASSERT_EQ(GetPasswordCount(i), init_password_count + num_clients()) <<
        "Total password count is wrong.";
  }
}

// We instantiate every test 4 times, for every combination of USS being enabled
// in individual clients. This verifies backward-compatibility between the two
// implementations.
INSTANTIATE_TEST_SUITE_P(USS,
                         TwoClientPasswordsSyncTest,
                         ::testing::Combine(::testing::Values(false, true),
                                            ::testing::Values(false, true)));
