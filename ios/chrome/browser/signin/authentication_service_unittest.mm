// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/device_accounts_synchronizer.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/test_identity_manager_observer.h"
#include "components/sync/driver/mock_sync_service.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/unified_consent/feature.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state_manager.h"
#include "ios/chrome/browser/content_settings/cookie_settings_factory.h"
#include "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/prefs/browser_prefs.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_delegate_fake.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service_mock.h"
#include "ios/chrome/browser/system_flags.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using testing::_;
using testing::Invoke;
using testing::Return;

namespace {

std::unique_ptr<KeyedService> BuildMockSyncService(web::BrowserState* context) {
  return std::make_unique<syncer::MockSyncService>();
}

std::unique_ptr<KeyedService> BuildMockSyncSetupService(
    web::BrowserState* context) {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<SyncSetupServiceMock>(
      ProfileSyncServiceFactory::GetForBrowserState(browser_state));
}

}  // namespace

class AuthenticationServiceTest : public PlatformTest {
 protected:
  AuthenticationServiceTest() {
    identity_service()->AddIdentities(@[ @"foo", @"foo2" ]);

    TestChromeBrowserState::Builder builder;
    builder.SetPrefService(CreatePrefService());
    builder.AddTestingFactory(ProfileSyncServiceFactory::GetInstance(),
                              base::BindRepeating(&BuildMockSyncService));
    builder.AddTestingFactory(SyncSetupServiceFactory::GetInstance(),
                              base::BindRepeating(&BuildMockSyncSetupService));
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());

    browser_state_ = builder.Build();

    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        browser_state_.get(),
        std::make_unique<AuthenticationServiceDelegateFake>());
    // Account ID migration is done on iOS.
    DCHECK_EQ(signin::IdentityManager::MIGRATION_DONE,
              identity_manager()->GetAccountIdMigrationState());
  }

  std::unique_ptr<sync_preferences::PrefServiceSyncable> CreatePrefService() {
    sync_preferences::PrefServiceMockFactory factory;
    scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
        new user_prefs::PrefRegistrySyncable);
    std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs =
        factory.CreateSyncable(registry.get());
    RegisterBrowserStatePrefs(registry.get());
    return prefs;
  }

  void SetExpectationsForSignIn() {
    EXPECT_CALL(*mock_sync_service()->GetMockUserSettings(),
                SetSyncRequested(true));
    EXPECT_CALL(*sync_setup_service_mock(), PrepareForFirstSyncSetup());
  }

  void StoreAccountsInPrefs() {
    authentication_service()->StoreAccountsInPrefs();
  }

  std::vector<std::string> GetAccountsInPrefs() {
    return authentication_service()->GetAccountsInPrefs();
  }

  void FireApplicationWillEnterForeground() {
    authentication_service()->OnApplicationWillEnterForeground();
  }

  void FireApplicationDidEnterBackground() {
    authentication_service()->OnApplicationDidEnterBackground();
  }

  void FireAccessTokenRefreshFailed(ChromeIdentity* identity,
                                    NSDictionary* user_info) {
    authentication_service()->OnAccessTokenRefreshFailed(identity, user_info);
  }

  void FireIdentityListChanged() {
    authentication_service()->OnIdentityListChanged();
  }

  void SetCachedMDMInfo(ChromeIdentity* identity, NSDictionary* user_info) {
    authentication_service()
        ->cached_mdm_infos_[base::SysNSStringToUTF8([identity gaiaID])] =
        user_info;
  }

  bool HasCachedMDMInfo(ChromeIdentity* identity) {
    return authentication_service()->cached_mdm_infos_.count(
               base::SysNSStringToUTF8([identity gaiaID])) > 0;
  }

  AuthenticationService* authentication_service() {
    return AuthenticationServiceFactory::GetForBrowserState(
        browser_state_.get());
  }

  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForBrowserState(browser_state_.get());
  }

  ios::FakeChromeIdentityService* identity_service() {
    return ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
  }

  syncer::MockSyncService* mock_sync_service() {
    return static_cast<syncer::MockSyncService*>(
        ProfileSyncServiceFactory::GetForBrowserState(browser_state_.get()));
  }

  SyncSetupServiceMock* sync_setup_service_mock() {
    return static_cast<SyncSetupServiceMock*>(
        SyncSetupServiceFactory::GetForBrowserState(browser_state_.get()));
  }

  ChromeIdentity* identity(NSUInteger index) {
    return [identity_service()->GetAllIdentitiesSortedForDisplay()
        objectAtIndex:index];
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
};

TEST_F(AuthenticationServiceTest, TestDefaultGetAuthenticatedIdentity) {
  EXPECT_FALSE(authentication_service()->GetAuthenticatedIdentity());
}

TEST_F(AuthenticationServiceTest, TestSignInAndGetAuthenticatedIdentity) {
  // Sign in.
  SetExpectationsForSignIn();
  authentication_service()->SignIn(identity(0));

  EXPECT_NSEQ(identity(0),
              authentication_service()->GetAuthenticatedIdentity());

  std::string user_email = base::SysNSStringToUTF8([identity(0) userEmail]);
  AccountInfo account_info =
      identity_manager()
          ->FindExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
              user_email)
          .value();
  EXPECT_EQ(user_email, account_info.email);
  EXPECT_EQ(base::SysNSStringToUTF8([identity(0) gaiaID]), account_info.gaia);
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(account_info.account_id));
}

TEST_F(AuthenticationServiceTest, TestSetPromptForSignIn) {
  // Verify that the default value of this flag is off.
  EXPECT_FALSE(authentication_service()->ShouldPromptForSignIn());
  // Verify that prompt-flag setter and getter functions are working correctly.
  authentication_service()->SetPromptForSignIn();
  EXPECT_TRUE(authentication_service()->ShouldPromptForSignIn());
  authentication_service()->ResetPromptForSignIn();
  EXPECT_FALSE(authentication_service()->ShouldPromptForSignIn());
}

TEST_F(AuthenticationServiceTest, OnAppEnterForegroundWithSyncSetupCompleted) {
  if (unified_consent::IsUnifiedConsentFeatureEnabled()) {
    // Authentication Service does not force sign the user our during its
    // initialization when Unified Consent feature is enabled. So this tests
    // is meaningless when Unfied Consent is enabled.
    return;
  }

  // Sign in.
  SetExpectationsForSignIn();
  authentication_service()->SignIn(identity(0));

  EXPECT_CALL(*sync_setup_service_mock(), HasFinishedInitialSetup())
      .WillOnce(Return(true));

  EXPECT_EQ(base::SysNSStringToUTF8([identity(0) userEmail]),
            identity_manager()->GetPrimaryAccountInfo().email);
  EXPECT_EQ(identity(0), authentication_service()->GetAuthenticatedIdentity());
}

TEST_F(AuthenticationServiceTest, OnAppEnterForegroundWithSyncDisabled) {
  if (unified_consent::IsUnifiedConsentFeatureEnabled()) {
    // Authentication Service does not force sign the user our during its
    // initialization when Unified Consent feature is enabled. So this tests
    // is meaningless when Unfied Consent is enabled.
    return;
  }

  // Sign in.
  SetExpectationsForSignIn();
  authentication_service()->SignIn(identity(0));

  EXPECT_CALL(*sync_setup_service_mock(), HasFinishedInitialSetup())
      .WillOnce(Invoke(
          sync_setup_service_mock(),
          &SyncSetupServiceMock::SyncSetupServiceHasFinishedInitialSetup));
  EXPECT_CALL(*mock_sync_service(), GetDisableReasons())
      .WillOnce(Return(syncer::SyncService::DISABLE_REASON_USER_CHOICE));

  EXPECT_EQ(base::SysNSStringToUTF8([identity(0) userEmail]),
            identity_manager()->GetPrimaryAccountInfo().email);
  EXPECT_EQ(identity(0), authentication_service()->GetAuthenticatedIdentity());
}

TEST_F(AuthenticationServiceTest, OnAppEnterForegroundWithSyncNotConfigured) {
  if (unified_consent::IsUnifiedConsentFeatureEnabled()) {
    // Authentication Service does not force sign the user our when the app
    // enters when Unified Consent feature is enabled. So this tests
    // is meaningless when Unfied Consent is enabled.
    return;
  }

  // Sign in.
  SetExpectationsForSignIn();
  authentication_service()->SignIn(identity(0));

  EXPECT_CALL(*sync_setup_service_mock(), HasFinishedInitialSetup())
      .WillOnce(Return(false));
  // Expect a call to disable sync as part of the sign out process.
  EXPECT_CALL(*mock_sync_service(), StopAndClear());

  // User is signed out if sync initial setup isn't completed.
  EXPECT_TRUE(identity_manager()->GetPrimaryAccountInfo().email.empty());
  EXPECT_FALSE(authentication_service()->GetAuthenticatedIdentity());
}

TEST_F(AuthenticationServiceTest, TestHandleForgottenIdentityNoPromptSignIn) {
  // Sign in.
  SetExpectationsForSignIn();
  authentication_service()->SignIn(identity(0));

  // Set the authentication service as "In Foreground", remove identity and run
  // the loop.
  FireApplicationDidEnterBackground();
  FireApplicationWillEnterForeground();
  identity_service()->ForgetIdentity(identity(0), nil);
  base::RunLoop().RunUntilIdle();

  // User is signed out (no corresponding identity), but not prompted for sign
  // in (as the action was user initiated).
  EXPECT_TRUE(identity_manager()->GetPrimaryAccountInfo().email.empty());
  EXPECT_FALSE(authentication_service()->GetAuthenticatedIdentity());
  EXPECT_FALSE(authentication_service()->ShouldPromptForSignIn());
}

TEST_F(AuthenticationServiceTest, TestHandleForgottenIdentityPromptSignIn) {
  // Sign in.
  SetExpectationsForSignIn();
  authentication_service()->SignIn(identity(0));

  // Set the authentication service as "In Background", remove identity and run
  // the loop.
  FireApplicationDidEnterBackground();
  identity_service()->ForgetIdentity(identity(0), nil);
  base::RunLoop().RunUntilIdle();

  // User is signed out (no corresponding identity), but not prompted for sign
  // in (as the action was user initiated).
  EXPECT_TRUE(identity_manager()->GetPrimaryAccountInfo().email.empty());
  EXPECT_FALSE(authentication_service()->GetAuthenticatedIdentity());
  EXPECT_TRUE(authentication_service()->ShouldPromptForSignIn());
}

TEST_F(AuthenticationServiceTest, StoreAndGetAccountsInPrefs) {
  // Profile starts empty.
  std::vector<std::string> accounts = GetAccountsInPrefs();
  EXPECT_TRUE(accounts.empty());

  // Sign in.
  SetExpectationsForSignIn();
  authentication_service()->SignIn(identity(0));

  // Store the accounts and get them back from the prefs. They should be the
  // same as the token service accounts.
  StoreAccountsInPrefs();
  accounts = GetAccountsInPrefs();
  ASSERT_EQ(2u, accounts.size());

      EXPECT_EQ("foo2ID", accounts[0]);
      EXPECT_EQ("fooID", accounts[1]);
}

TEST_F(AuthenticationServiceTest,
       OnApplicationEnterForegroundReloadCredentials) {
  // Sign in.
  SetExpectationsForSignIn();
  authentication_service()->SignIn(identity(0));

  identity_service()->AddIdentities(@[ @"foo3" ]);

  auto account_compare_func = [](const CoreAccountInfo& first,
                                 const CoreAccountInfo& second) {
    return first.account_id < second.account_id;
  };
  std::vector<CoreAccountInfo> accounts =
      identity_manager()->GetAccountsWithRefreshTokens();
  std::sort(accounts.begin(), accounts.end(), account_compare_func);
  ASSERT_EQ(2u, accounts.size());

      EXPECT_EQ("foo2ID", accounts[0].account_id);
      EXPECT_EQ("fooID", accounts[1].account_id);

  // Simulate a switching to background and back to foreground, triggering a
  // credentials reload.
  FireApplicationDidEnterBackground();
  FireApplicationWillEnterForeground();

  // Accounts are reloaded, "foo3@foo.com" is added as it is now in
  // ChromeIdentityService.
  accounts = identity_manager()->GetAccountsWithRefreshTokens();
  std::sort(accounts.begin(), accounts.end(), account_compare_func);
  ASSERT_EQ(3u, accounts.size());
      EXPECT_EQ("foo2ID", accounts[0].account_id);
      EXPECT_EQ("foo3ID", accounts[1].account_id);
      EXPECT_EQ("fooID", accounts[2].account_id);
}

TEST_F(AuthenticationServiceTest, HaveAccountsNotChangedDefault) {
  EXPECT_FALSE(authentication_service()->HaveAccountsChanged());
}

TEST_F(AuthenticationServiceTest, HaveAccountsNotChanged) {
  SetExpectationsForSignIn();
  authentication_service()->SignIn(identity(0));

  identity_service()->AddIdentities(@[ @"foo3" ]);
  FireIdentityListChanged();
  base::RunLoop().RunUntilIdle();

  // Simulate a switching to background and back to foreground.
  FireApplicationDidEnterBackground();
  FireApplicationWillEnterForeground();

  EXPECT_FALSE(authentication_service()->HaveAccountsChanged());
}

TEST_F(AuthenticationServiceTest, HaveAccountsChanged) {
  SetExpectationsForSignIn();
  authentication_service()->SignIn(identity(0));

  identity_service()->AddIdentities(@[ @"foo3" ]);
  FireIdentityListChanged();
  base::RunLoop().RunUntilIdle();

  // Simulate a switching to background and back to foreground, changing the
  // accounts while in background.
  FireApplicationDidEnterBackground();
  identity_service()->AddIdentities(@[ @"foo4" ]);
  FireApplicationWillEnterForeground();

  EXPECT_TRUE(authentication_service()->HaveAccountsChanged());
}

TEST_F(AuthenticationServiceTest, HaveAccountsChangedBackground) {
  SetExpectationsForSignIn();
  authentication_service()->SignIn(identity(0));

  identity_service()->AddIdentities(@[ @"foo3" ]);
  FireIdentityListChanged();
  base::RunLoop().RunUntilIdle();

  // Simulate a switching to background, changing the accounts while in
  // background.
  FireApplicationDidEnterBackground();
  identity_service()->AddIdentities(@[ @"foo4" ]);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(authentication_service()->HaveAccountsChanged());
}

TEST_F(AuthenticationServiceTest, IsAuthenticatedBackground) {
  // Sign in.
  SetExpectationsForSignIn();
  authentication_service()->SignIn(identity(0));
  EXPECT_TRUE(authentication_service()->IsAuthenticated());

  // Remove the signed in identity while in background, and check that
  // IsAuthenticated is up-to-date.
  FireApplicationDidEnterBackground();
  identity_service()->ForgetIdentity(identity(0), nil);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(authentication_service()->IsAuthenticated());
}

// Tests that MDM errors are correctly cleared on foregrounding, sending
// notifications that the state of error has changed.
TEST_F(AuthenticationServiceTest, MDMErrorsClearedOnForeground) {
  SetExpectationsForSignIn();
  authentication_service()->SignIn(identity(0));
  EXPECT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 2UL);

  NSDictionary* user_info = [NSDictionary dictionary];
  SetCachedMDMInfo(identity(0), user_info);
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager(), base::SysNSStringToUTF8([identity(0) gaiaID]), error);

  // MDM error for |identity_| is being cleared and the error state of refresh
  // token will be updated.
  {
    bool notification_received = false;
    signin::TestIdentityManagerObserver observer(identity_manager());
    observer.SetOnErrorStateOfRefreshTokenUpdatedCallback(
        base::BindLambdaForTesting([&]() { notification_received = true; }));

    FireApplicationDidEnterBackground();
    FireApplicationWillEnterForeground();
    EXPECT_TRUE(notification_received);
    EXPECT_EQ(
        base::SysNSStringToUTF8([identity(0) gaiaID]),
        observer.AccountFromErrorStateOfRefreshTokenUpdatedCallback().gaia);
  }

  // MDM error has already been cleared, no notification will be sent.
  {
    bool notification_received = false;
    signin::TestIdentityManagerObserver observer(identity_manager());
    observer.SetOnErrorStateOfRefreshTokenUpdatedCallback(
        base::BindLambdaForTesting([&]() { notification_received = true; }));

    FireApplicationDidEnterBackground();
    FireApplicationWillEnterForeground();
    EXPECT_FALSE(notification_received);
  }
}

// Tests that MDM errors are correctly cleared when signing out.
TEST_F(AuthenticationServiceTest, MDMErrorsClearedOnSignout) {
  SetExpectationsForSignIn();
  authentication_service()->SignIn(identity(0));
  EXPECT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 2UL);

  NSDictionary* user_info = [NSDictionary dictionary];
  SetCachedMDMInfo(identity(0), user_info);

  authentication_service()->SignOut(signin_metrics::ABORT_SIGNIN, nil);
  EXPECT_FALSE(HasCachedMDMInfo(identity(0)));
  EXPECT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 0UL);
}

// Tests that potential MDM notifications are correctly handled and dispatched
// to MDM service when necessary.
TEST_F(AuthenticationServiceTest, HandleMDMNotification) {
  SetExpectationsForSignIn();
  authentication_service()->SignIn(identity(0));
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager(), base::SysNSStringToUTF8([identity(0) gaiaID]), error);

  NSDictionary* user_info1 = @{ @"foo" : @1 };
  ON_CALL(*identity_service(), GetMDMDeviceStatus(user_info1))
      .WillByDefault(Return(1));
  NSDictionary* user_info2 = @{ @"foo" : @2 };
  ON_CALL(*identity_service(), GetMDMDeviceStatus(user_info2))
      .WillByDefault(Return(2));

  // Notification will show the MDM dialog the first time.
  EXPECT_CALL(*identity_service(),
              HandleMDMNotification(identity(0), user_info1, _))
      .WillOnce(Return(true));
  FireAccessTokenRefreshFailed(identity(0), user_info1);

  // Same notification won't show the MDM dialog the second time.
  EXPECT_CALL(*identity_service(),
              HandleMDMNotification(identity(0), user_info1, _))
      .Times(0);
  FireAccessTokenRefreshFailed(identity(0), user_info1);

  // New notification will show the MDM dialog on the same identity.
  EXPECT_CALL(*identity_service(),
              HandleMDMNotification(identity(0), user_info2, _))
      .WillOnce(Return(true));
  FireAccessTokenRefreshFailed(identity(0), user_info2);
}

// Tests that MDM blocked notifications are correctly signing out the user if
// the primary account is blocked.
TEST_F(AuthenticationServiceTest, HandleMDMBlockedNotification) {
  SetExpectationsForSignIn();
  authentication_service()->SignIn(identity(0));
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager(), base::SysNSStringToUTF8([identity(0) gaiaID]), error);

  NSDictionary* user_info1 = @{ @"foo" : @1 };
  ON_CALL(*identity_service(), GetMDMDeviceStatus(user_info1))
      .WillByDefault(Return(1));

  auto handle_mdm_notification_callback = [](ChromeIdentity*, NSDictionary*,
                                             ios::MDMStatusCallback callback) {
    callback(true /* is_blocked */);
    return true;
  };

  // User not signed out as |identity(1)| isn't the primary account.
  EXPECT_CALL(*identity_service(),
              HandleMDMNotification(identity(1), user_info1, _))
      .WillOnce(Invoke(handle_mdm_notification_callback));
  FireAccessTokenRefreshFailed(identity(1), user_info1);
  EXPECT_TRUE(authentication_service()->IsAuthenticated());

  // User signed out as |identity_| is the primary account.
  EXPECT_CALL(*identity_service(),
              HandleMDMNotification(identity(0), user_info1, _))
      .WillOnce(Invoke(handle_mdm_notification_callback));
  FireAccessTokenRefreshFailed(identity(0), user_info1);
  EXPECT_FALSE(authentication_service()->IsAuthenticated());
}

// Tests that MDM dialog isn't shown when there is no cached MDM error.
TEST_F(AuthenticationServiceTest, ShowMDMErrorDialogNoCachedError) {
  EXPECT_CALL(*identity_service(), HandleMDMNotification(identity(0), _, _))
      .Times(0);

  EXPECT_FALSE(
      authentication_service()->ShowMDMErrorDialogForIdentity(identity(0)));
}

// Tests that MDM dialog isn't shown when there is a cached MDM error but no
// corresponding error for the account.
TEST_F(AuthenticationServiceTest, ShowMDMErrorDialogInvalidCachedError) {
  NSDictionary* user_info = [NSDictionary dictionary];
  SetCachedMDMInfo(identity(0), user_info);

  EXPECT_CALL(*identity_service(),
              HandleMDMNotification(identity(0), user_info, _))
      .Times(0);

  EXPECT_FALSE(
      authentication_service()->ShowMDMErrorDialogForIdentity(identity(0)));
}

// Tests that MDM dialog is shown when there is a cached error and a
// corresponding error for the account.
TEST_F(AuthenticationServiceTest, ShowMDMErrorDialog) {
  SetExpectationsForSignIn();
  authentication_service()->SignIn(identity(0));
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager(), base::SysNSStringToUTF8([identity(0) gaiaID]), error);

  NSDictionary* user_info = [NSDictionary dictionary];
  SetCachedMDMInfo(identity(0), user_info);

  EXPECT_CALL(*identity_service(),
              HandleMDMNotification(identity(0), user_info, _))
      .WillOnce(Return(true));

  EXPECT_TRUE(
      authentication_service()->ShowMDMErrorDialogForIdentity(identity(0)));
}
