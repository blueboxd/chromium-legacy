// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>
#include <string>

#include "base/bind.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/sync/engine/sync_engine.h"
#include "components/unified_consent/feature.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using syncer::TestSyncService;

namespace {

// A number of distinct states of the SyncService can be generated for tests.
enum DistinctState {
  STATUS_CASE_SETUP_IN_PROGRESS,
  STATUS_CASE_SETUP_ERROR,
  STATUS_CASE_AUTH_ERROR,
  STATUS_CASE_PROTOCOL_ERROR,
  STATUS_CASE_CONFIRM_SYNC_SETTINGS,
  STATUS_CASE_PASSPHRASE_ERROR,
  STATUS_CASE_SYNCED,
  STATUS_CASE_SYNC_DISABLED_BY_POLICY,
  STATUS_CASE_SYNC_RESET_FROM_DASHBOARD,
  NUMBER_OF_STATUS_CASES
};

const char kTestUser[] = "test_user@test.com";

}  // namespace

class SyncUIUtilTest : public testing::Test {
 private:
  content::TestBrowserThreadBundle thread_bundle_;
};

// Sets up a TestSyncService to emulate one of a number of distinct cases in
// order to perform tests on the generated messages. Returns the expected values
// for the MessageType and ActionType that sync_ui_util::GetStatusLabels should
// return.
std::pair<sync_ui_util::MessageType, sync_ui_util::ActionType>
SetUpDistinctCase(TestSyncService* service,
                  signin::IdentityManager* identity_manager,
                  int case_number) {
  switch (case_number) {
    case STATUS_CASE_SETUP_IN_PROGRESS: {
      service->SetFirstSetupComplete(false);
      service->SetSetupInProgress(true);
      service->SetDetailedSyncStatus(false, syncer::SyncStatus());
      return std::make_tuple(sync_ui_util::PRE_SYNCED, sync_ui_util::NO_ACTION);
    }
    case STATUS_CASE_SETUP_ERROR: {
      service->SetFirstSetupComplete(false);
      service->SetSetupInProgress(false);
      service->SetDisableReasons(
          syncer::SyncService::DISABLE_REASON_UNRECOVERABLE_ERROR);
      service->SetDetailedSyncStatus(false, syncer::SyncStatus());
      return std::make_tuple(sync_ui_util::SYNC_ERROR,
                             sync_ui_util::REAUTHENTICATE);
    }
    case STATUS_CASE_AUTH_ERROR: {
      service->SetFirstSetupComplete(true);
      service->SetTransportState(syncer::SyncService::TransportState::ACTIVE);
      service->SetPassphraseRequired(false);
      service->SetDetailedSyncStatus(false, syncer::SyncStatus());

      // Make sure to fail authentication with an error in this case.
      std::string account_id = identity_manager->GetPrimaryAccountId();
      signin::SetRefreshTokenForPrimaryAccount(identity_manager);
      service->SetAuthenticatedAccountInfo(
          identity_manager->GetPrimaryAccountInfo());
      signin::UpdatePersistentErrorOfRefreshTokenForAccount(
          identity_manager, account_id,
          GoogleServiceAuthError(GoogleServiceAuthError::State::SERVICE_ERROR));
      service->SetDisableReasons(syncer::SyncService::DISABLE_REASON_NONE);
      return std::make_tuple(sync_ui_util::SYNC_ERROR,
                             sync_ui_util::REAUTHENTICATE);
    }
    case STATUS_CASE_PROTOCOL_ERROR: {
      service->SetFirstSetupComplete(true);
      service->SetTransportState(syncer::SyncService::TransportState::ACTIVE);
      service->SetPassphraseRequired(false);
      syncer::SyncProtocolError protocol_error;
      protocol_error.action = syncer::UPGRADE_CLIENT;
      syncer::SyncStatus status;
      status.sync_protocol_error = protocol_error;
      service->SetDetailedSyncStatus(false, status);
      service->SetDisableReasons(syncer::SyncService::DISABLE_REASON_NONE);
      return std::make_tuple(sync_ui_util::SYNC_ERROR,
                             sync_ui_util::UPGRADE_CLIENT);
    }
    case STATUS_CASE_CONFIRM_SYNC_SETTINGS: {
      service->SetFirstSetupComplete(false);
      service->SetPassphraseRequired(false);
      service->SetDetailedSyncStatus(false, syncer::SyncStatus());
      return std::make_tuple(sync_ui_util::SYNC_ERROR,
                             sync_ui_util::CONFIRM_SYNC_SETTINGS);
    }
    case STATUS_CASE_PASSPHRASE_ERROR: {
      service->SetFirstSetupComplete(true);
      service->SetTransportState(syncer::SyncService::TransportState::ACTIVE);
      service->SetDetailedSyncStatus(false, syncer::SyncStatus());
      service->SetDisableReasons(syncer::SyncService::DISABLE_REASON_NONE);
      service->SetPassphraseRequired(true);
      service->SetPassphraseRequiredForDecryption(true);
      return std::make_tuple(sync_ui_util::SYNC_ERROR,
                             sync_ui_util::ENTER_PASSPHRASE);
    }
    case STATUS_CASE_SYNCED: {
      service->SetFirstSetupComplete(true);
      service->SetTransportState(syncer::SyncService::TransportState::ACTIVE);
      service->SetDetailedSyncStatus(false, syncer::SyncStatus());
      service->SetDisableReasons(syncer::SyncService::DISABLE_REASON_NONE);
      service->SetPassphraseRequired(false);
      return std::make_tuple(sync_ui_util::SYNCED, sync_ui_util::NO_ACTION);
    }
    case STATUS_CASE_SYNC_DISABLED_BY_POLICY: {
      service->SetDisableReasons(
          syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY);
      service->SetFirstSetupComplete(false);
      service->SetTransportState(syncer::SyncService::TransportState::DISABLED);
      service->SetPassphraseRequired(false);
      service->SetDetailedSyncStatus(false, syncer::SyncStatus());
      return std::make_tuple(sync_ui_util::SYNCED, sync_ui_util::NO_ACTION);
    }
    case STATUS_CASE_SYNC_RESET_FROM_DASHBOARD: {
      // Note: On desktop, if there is a primary account, then
      // DISABLE_REASON_USER_CHOICE can only occur if Sync was reset from the
      // dashboard, and the UI treats it as such.
      service->SetDisableReasons(
          syncer::SyncService::DISABLE_REASON_USER_CHOICE);
      service->SetFirstSetupComplete(true);
      service->SetTransportState(syncer::SyncService::TransportState::ACTIVE);
      service->SetPassphraseRequired(false);
      service->SetDetailedSyncStatus(false, syncer::SyncStatus());
      // This case gets different treatment depending on whether UnifiedConsent
      // is enabled, see crbug.com/943983 and crbug.com/977980.
      sync_ui_util::MessageType expected_message_type =
          unified_consent::IsUnifiedConsentFeatureEnabled()
              ? sync_ui_util::SYNC_ERROR
              : sync_ui_util::PRE_SYNCED;
      return std::make_tuple(expected_message_type, sync_ui_util::NO_ACTION);
    }
    case NUMBER_OF_STATUS_CASES:
      NOTREACHED();
  }
  return std::make_tuple(sync_ui_util::PRE_SYNCED, sync_ui_util::NO_ACTION);
}

std::unique_ptr<KeyedService> BuildTestSyncService(
    content::BrowserContext* context) {
  return std::make_unique<TestSyncService>();
}

std::unique_ptr<TestingProfile> BuildTestingProfile() {
  return IdentityTestEnvironmentProfileAdaptor::
      CreateProfileForIdentityTestEnvironment(
          {{ProfileSyncServiceFactory::GetInstance(),
            base::BindRepeating(&BuildTestSyncService)}});
}

std::unique_ptr<TestingProfile> BuildSignedInTestingProfile() {
  std::unique_ptr<TestingProfile> profile = BuildTestingProfile();
  IdentityTestEnvironmentProfileAdaptor identity_adaptor(profile.get());
  identity_adaptor.identity_test_env()->SetPrimaryAccount(kTestUser);
  return profile;
}

// This test ensures that each distinctive SyncService status will return a
// unique combination of status and link messages from GetStatusLabels().
TEST_F(SyncUIUtilTest, DistinctCasesReportUniqueMessageSets) {
  std::set<base::string16> messages;
  for (int idx = 0; idx != NUMBER_OF_STATUS_CASES; idx++) {
    std::unique_ptr<Profile> profile = BuildTestingProfile();

    IdentityTestEnvironmentProfileAdaptor env_adaptor(profile.get());
    signin::IdentityTestEnvironment* environment =
        env_adaptor.identity_test_env();
    signin::IdentityManager* identity_manager = environment->identity_manager();

    // Need a primary account signed in before calling SetUpDistinctCase().
    environment->MakePrimaryAccountAvailable(kTestUser);

    TestSyncService* service = static_cast<TestSyncService*>(
        ProfileSyncServiceFactory::GetForProfile(profile.get()));
    sync_ui_util::MessageType expected_message_type;
    sync_ui_util::ActionType expected_action_type;
    std::tie(expected_message_type, expected_action_type) =
        SetUpDistinctCase(service, identity_manager, idx);
    base::string16 status_label;
    base::string16 link_label;
    sync_ui_util::ActionType action_type = sync_ui_util::NO_ACTION;
    sync_ui_util::MessageType message_type = sync_ui_util::GetStatusLabels(
        profile.get(), &status_label, &link_label, &action_type);

    EXPECT_EQ(expected_message_type, message_type)
        << "Wrong message type returned for case #" << idx;
    EXPECT_EQ(expected_action_type, action_type)
        << "Wrong action returned for case #" << idx;
    // If the status and link message combination is already present in the set
    // of messages already seen, this is a duplicate rather than a unique
    // message, and the test has failed.
    EXPECT_FALSE(status_label.empty())
        << "Empty status label returned for case #" << idx;
    // Ensures a search for string 'href' (found in links, not a string to be
    // found in an English language message) fails, since links are excluded
    // from the status label.
    EXPECT_EQ(status_label.find(base::ASCIIToUTF16("href")),
              base::string16::npos);
    base::string16 combined_label =
        status_label + base::ASCIIToUTF16("#") + link_label;
    EXPECT_TRUE(messages.find(combined_label) == messages.end()) <<
        "Duplicate message for case #" << idx << ": " << combined_label;
    messages.insert(combined_label);
  }
}

TEST_F(SyncUIUtilTest, UnrecoverableErrorWithActionableError) {
  std::unique_ptr<Profile> profile = BuildSignedInTestingProfile();

  TestSyncService* service = static_cast<TestSyncService*>(
      ProfileSyncServiceFactory::GetForProfile(profile.get()));
  service->SetFirstSetupComplete(true);
  service->SetDisableReasons(
      syncer::SyncService::DISABLE_REASON_UNRECOVERABLE_ERROR);

  // First time action is not set. We should get unrecoverable error.
  service->SetDetailedSyncStatus(true, syncer::SyncStatus());

  base::string16 link_label;
  base::string16 unrecoverable_error_status_label;
  sync_ui_util::ActionType action_type = sync_ui_util::NO_ACTION;
  sync_ui_util::GetStatusLabels(profile.get(),
                                &unrecoverable_error_status_label, &link_label,
                                &action_type);

  // Expect the generic unrecoverable error action which is to reauthenticate.
  EXPECT_EQ(sync_ui_util::REAUTHENTICATE, action_type);

  // This time set action to UPGRADE_CLIENT. Ensure that status label differs
  // from previous one.
  syncer::SyncStatus status;
  status.sync_protocol_error.action = syncer::UPGRADE_CLIENT;
  service->SetDetailedSyncStatus(true, status);
  base::string16 upgrade_client_status_label;
  sync_ui_util::GetStatusLabels(profile.get(), &upgrade_client_status_label,
                                &link_label, &action_type);
  // Expect an explicit 'client upgrade' action.
  EXPECT_EQ(sync_ui_util::UPGRADE_CLIENT, action_type);

  EXPECT_NE(unrecoverable_error_status_label, upgrade_client_status_label);
}

TEST_F(SyncUIUtilTest, ActionableErrorWithPassiveMessage) {
  std::unique_ptr<Profile> profile = BuildSignedInTestingProfile();

  TestSyncService* service = static_cast<TestSyncService*>(
      ProfileSyncServiceFactory::GetForProfile(profile.get()));
  service->SetFirstSetupComplete(true);
  service->SetDisableReasons(
      syncer::SyncService::DISABLE_REASON_UNRECOVERABLE_ERROR);

  // Set action to UPGRADE_CLIENT.
  syncer::SyncStatus status;
  status.sync_protocol_error.action = syncer::UPGRADE_CLIENT;
  service->SetDetailedSyncStatus(true, status);

  base::string16 actionable_error_status_label;
  base::string16 link_label;
  sync_ui_util::ActionType action_type = sync_ui_util::NO_ACTION;
  sync_ui_util::GetStatusLabels(profile.get(), &actionable_error_status_label,
                                &link_label, &action_type);
  // Expect a 'client upgrade' call to action.
  EXPECT_EQ(sync_ui_util::UPGRADE_CLIENT, action_type);
  EXPECT_NE(actionable_error_status_label, base::string16());
}

TEST_F(SyncUIUtilTest, SyncSettingsConfirmationNeededTest) {
  std::unique_ptr<Profile> profile = BuildSignedInTestingProfile();

  TestSyncService* service = static_cast<TestSyncService*>(
      ProfileSyncServiceFactory::GetForProfile(profile.get()));
  service->SetFirstSetupComplete(false);
  ASSERT_TRUE(sync_ui_util::ShouldRequestSyncConfirmation(service));

  base::string16 actionable_error_status_label;
  base::string16 link_label;
  sync_ui_util::ActionType action_type = sync_ui_util::NO_ACTION;

  sync_ui_util::GetStatusLabels(profile.get(), &actionable_error_status_label,
                                &link_label, &action_type);

  EXPECT_EQ(action_type, sync_ui_util::CONFIRM_SYNC_SETTINGS);
}

// Errors in non-sync accounts should be ignored.
TEST_F(SyncUIUtilTest, IgnoreSyncErrorForNonSyncAccount) {
  std::unique_ptr<Profile> profile = BuildTestingProfile();

  IdentityTestEnvironmentProfileAdaptor env_adaptor(profile.get());
  signin::IdentityTestEnvironment* environment =
      env_adaptor.identity_test_env();
  signin::IdentityManager* identity_manager = environment->identity_manager();
  AccountInfo primary_account_info =
      environment->MakePrimaryAccountAvailable(kTestUser);

  TestSyncService* service = static_cast<TestSyncService*>(
      ProfileSyncServiceFactory::GetForProfile(profile.get()));
  service->SetAuthenticatedAccountInfo(primary_account_info);
  service->SetFirstSetupComplete(true);

  // Setup a secondary account.
  AccountInfo secondary_account_info =
      environment->MakeAccountAvailable("secondary-user@example.com");

  // Verify that we do not have any existing errors.
  base::string16 actionable_error_status_label;
  base::string16 link_label;
  sync_ui_util::ActionType action_type = sync_ui_util::NO_ACTION;

  sync_ui_util::MessageType message = sync_ui_util::GetStatusLabels(
      profile.get(), &actionable_error_status_label, &link_label, &action_type);

  EXPECT_EQ(action_type, sync_ui_util::NO_ACTION);
  EXPECT_EQ(message, sync_ui_util::MessageType::SYNCED);

  // Add an error to the secondary account.
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager, secondary_account_info.account_id,
      GoogleServiceAuthError(
          GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));

  // Verify that we do not see any sign-in errors.
  message = sync_ui_util::GetStatusLabels(
      profile.get(), &actionable_error_status_label, &link_label, &action_type);

  EXPECT_EQ(action_type, sync_ui_util::NO_ACTION);
  EXPECT_EQ(message, sync_ui_util::MessageType::SYNCED);
}
