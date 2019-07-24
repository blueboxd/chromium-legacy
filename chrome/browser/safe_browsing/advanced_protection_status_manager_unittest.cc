// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"

#include <string>

#include "base/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {
namespace {

static const char* kIdTokenAdvancedProtectionEnabled =
    "dummy-header."
    "eyAic2VydmljZXMiOiBbInRpYSJdIH0="  // payload: { "services": ["tia"] }
    ".dummy-signature";
static const char* kIdTokenAdvancedProtectionDisabled =
    "dummy-header."
    "eyAic2VydmljZXMiOiBbXSB9"  // payload: { "services": [] }
    ".dummy-signature";

static const char* kAPTokenFetchStatusMetric =
    "SafeBrowsing.AdvancedProtection.APTokenFetchStatus";
static const char* kTokenFetchStatusMetric =
    "SafeBrowsing.AdvancedProtection.TokenFetchStatus";

class AdvancedProtectionStatusManagerTest : public testing::Test {
 public:
  AdvancedProtectionStatusManagerTest() {
    testing_profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment();

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            testing_profile_.get());
  }

  ~AdvancedProtectionStatusManagerTest() override {}

  std::string SignIn(const std::string& email,
                     bool is_under_advanced_protection) {
    AccountInfo account_info = identity_test_env()->MakeAccountAvailable(email);

    account_info.is_under_advanced_protection = is_under_advanced_protection;
    identity_test_env()->SetPrimaryAccount(account_info.email);
    identity_test_env()->UpdateAccountInfoForAccount(account_info);

    return account_info.account_id;
  }

  void MakeOAuthTokenFetchSucceed(const std::string& account_id,
                                  bool is_under_advanced_protection) {
    identity_test_env()
        ->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
            account_id, "access_token",
            base::Time::Now() + base::TimeDelta::FromHours(1),
            is_under_advanced_protection ? kIdTokenAdvancedProtectionEnabled
                                         : kIdTokenAdvancedProtectionDisabled);
  }

  void MakeOAuthTokenFetchFail(const std::string& account_id,
                               bool is_transient_error) {
    identity_test_env()
        ->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
            account_id,
            GoogleServiceAuthError(
                is_transient_error
                    ? GoogleServiceAuthError::CONNECTION_FAILED
                    : GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle;
  std::unique_ptr<TestingProfile> testing_profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
};

}  // namespace

TEST_F(AdvancedProtectionStatusManagerTest, NotSignedInOnStartUp) {
  ASSERT_FALSE(testing_profile_->GetPrefs()->HasPrefPath(
      prefs::kAdvancedProtectionLastRefreshInUs));
  AdvancedProtectionStatusManager aps_manager(
      testing_profile_.get(), base::TimeDelta() /*no min delay*/);
  ASSERT_TRUE(aps_manager.GetPrimaryAccountId().empty());

  // If user's not signed-in. No refresh is required.
  EXPECT_FALSE(aps_manager.is_under_advanced_protection());
  EXPECT_FALSE(aps_manager.IsRefreshScheduled());
  EXPECT_FALSE(testing_profile_->GetPrefs()->HasPrefPath(
      prefs::kAdvancedProtectionLastRefreshInUs));
  aps_manager.UnsubscribeFromSigninEvents();
}

TEST_F(AdvancedProtectionStatusManagerTest,
       SignedInLongTimeAgoRefreshFailTransientError) {
  base::HistogramTester histograms;
  ASSERT_FALSE(testing_profile_->GetPrefs()->HasPrefPath(
      prefs::kAdvancedProtectionLastRefreshInUs));

  // Simulates the situation where user signed in long time ago, thus
  // has no advanced protection status.
  std::string account_id =
      SignIn("test@test.com", /* is_under_advanced_protection = */ false);
  AdvancedProtectionStatusManager aps_manager(
      testing_profile_.get(), base::TimeDelta() /*no min delay*/);
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(aps_manager.GetPrimaryAccountId().empty());

  // Waits for access token request and respond with an error without advanced
  // protection set.
  MakeOAuthTokenFetchFail(account_id, /* is_transient_error = */ true);
  EXPECT_FALSE(aps_manager.is_under_advanced_protection());

  EXPECT_THAT(histograms.GetAllSamples(kTokenFetchStatusMetric),
              testing::ElementsAre(base::Bucket(3 /*CONNECTION_FAILED*/, 1)));
  EXPECT_THAT(histograms.GetAllSamples(kAPTokenFetchStatusMetric),
              testing::IsEmpty());

  // A retry should be scheduled.
  EXPECT_TRUE(aps_manager.IsRefreshScheduled());
  EXPECT_FALSE(testing_profile_->GetPrefs()->HasPrefPath(
      prefs::kAdvancedProtectionLastRefreshInUs));
  aps_manager.UnsubscribeFromSigninEvents();
}

TEST_F(AdvancedProtectionStatusManagerTest,
       SignedInLongTimeAgoRefreshFailNonTransientError) {
  base::HistogramTester histograms;
  // Simulates the situation where user signed in long time ago, thus
  // has no advanced protection status.
  std::string account_id =
      SignIn("test@test.com", /* is_under_advanced_protection = */ false);
  AdvancedProtectionStatusManager aps_manager(
      testing_profile_.get(), base::TimeDelta() /*no min delay*/);
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(aps_manager.GetPrimaryAccountId().empty());

  // Waits for access token request and respond with an error without advanced
  // protection set.
  MakeOAuthTokenFetchFail(account_id, /* is_transient_error = */ false);
  EXPECT_FALSE(aps_manager.is_under_advanced_protection());

  EXPECT_THAT(
      histograms.GetAllSamples(kTokenFetchStatusMetric),
      testing::ElementsAre(base::Bucket(1 /*INVALID_GAIA_CREDENTIALS*/, 1)));
  EXPECT_THAT(histograms.GetAllSamples(kAPTokenFetchStatusMetric),
              testing::IsEmpty());

  // No retry should be scheduled.
  EXPECT_FALSE(aps_manager.IsRefreshScheduled());
  aps_manager.UnsubscribeFromSigninEvents();
}

TEST_F(AdvancedProtectionStatusManagerTest, SignedInLongTimeAgoNotUnderAP) {
  ASSERT_FALSE(testing_profile_->GetPrefs()->HasPrefPath(
      prefs::kAdvancedProtectionLastRefreshInUs));
  base::HistogramTester histograms;
  // Simulates the situation where user signed in long time ago, thus
  // has no advanced protection status.
  std::string account_id =
      SignIn("test@test.com", /* is_under_advanced_protection = */ false);
  AdvancedProtectionStatusManager aps_manager(
      testing_profile_.get(), base::TimeDelta() /*no min delay*/);
  ASSERT_FALSE(aps_manager.GetPrimaryAccountId().empty());
  base::RunLoop().RunUntilIdle();
  // Waits for access token request and respond with a token without advanced
  // protection set.
  MakeOAuthTokenFetchSucceed(account_id,
                             /* is_under_advanced_protection = */ false);

  EXPECT_FALSE(aps_manager.is_under_advanced_protection());
  EXPECT_FALSE(aps_manager.IsRefreshScheduled());
  EXPECT_TRUE(testing_profile_->GetPrefs()->HasPrefPath(
      prefs::kAdvancedProtectionLastRefreshInUs));

  EXPECT_THAT(histograms.GetAllSamples(kTokenFetchStatusMetric),
              testing::ElementsAre(base::Bucket(0 /*NONE*/, 1)));
  EXPECT_THAT(histograms.GetAllSamples(kAPTokenFetchStatusMetric),
              testing::IsEmpty());

  aps_manager.UnsubscribeFromSigninEvents();
}

TEST_F(AdvancedProtectionStatusManagerTest, SignedInLongTimeAgoUnderAP) {
  // Simulates the situation where user signed in long time ago, thus
  // has no advanced protection status yet.
  std::string account_id =
      SignIn("test@test.com", /* is_under_advanced_protection = */ false);
  AdvancedProtectionStatusManager aps_manager(
      testing_profile_.get(), base::TimeDelta() /*no min delay*/);
  base::RunLoop().RunUntilIdle();
  // Waits for access token request and respond with a token without advanced
  // protection set.
  MakeOAuthTokenFetchSucceed(account_id,
                             /* is_under_advanced_protection = */ true);

  EXPECT_TRUE(aps_manager.is_under_advanced_protection());
  EXPECT_TRUE(aps_manager.IsRefreshScheduled());
  EXPECT_TRUE(testing_profile_->GetPrefs()->HasPrefPath(
      prefs::kAdvancedProtectionLastRefreshInUs));
  aps_manager.UnsubscribeFromSigninEvents();
}

TEST_F(AdvancedProtectionStatusManagerTest, AlreadySignedInAndUnderAP) {
  testing_profile_->GetPrefs()->SetInt64(
      prefs::kAdvancedProtectionLastRefreshInUs,
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());

  // Simulates the situation where the user has already signed in and is
  // under advanced protection.
  std::string account_id =
      SignIn("test@test.com", /* is_under_advanced_protection = */ true);
  AdvancedProtectionStatusManager aps_manager(
      testing_profile_.get(), base::TimeDelta() /*no min delay*/);
  ASSERT_FALSE(aps_manager.GetPrimaryAccountId().empty());
  ASSERT_TRUE(aps_manager.is_under_advanced_protection());

  // A refresh is scheduled in the future.
  EXPECT_TRUE(aps_manager.IsRefreshScheduled());
  aps_manager.UnsubscribeFromSigninEvents();
}

TEST_F(AdvancedProtectionStatusManagerTest,
       AlreadySignedInAndUnderAPIncognito) {
  testing_profile_->GetPrefs()->SetInt64(
      prefs::kAdvancedProtectionLastRefreshInUs,
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());

  // Simulates the situation where the user has already signed in and is
  // under advanced protection.
  std::string account_id =
      SignIn("test@test.com", /* is_under_advanced_protection = */ true);
  AdvancedProtectionStatusManagerFactory::GetForBrowserContext(
      Profile::FromBrowserContext(testing_profile_.get()))
      ->MaybeRefreshOnStartUp();

  // Incognito profile should share the advanced protection status with the
  // original profile.
  EXPECT_TRUE(AdvancedProtectionStatusManager::IsUnderAdvancedProtection(
      testing_profile_->GetOffTheRecordProfile()));
  EXPECT_TRUE(AdvancedProtectionStatusManager::IsUnderAdvancedProtection(
      testing_profile_.get()));
}

TEST_F(AdvancedProtectionStatusManagerTest,
       AlreadySignedInAndNotUnderAPIncognito) {
  testing_profile_->GetPrefs()->SetInt64(
      prefs::kAdvancedProtectionLastRefreshInUs,
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());

  // Simulates the situation where the user has already signed in and is
  // NOT under advanced protection.
  std::string account_id =
      SignIn("test@test.com", /* is_under_advanced_protection = */ false);
  AdvancedProtectionStatusManagerFactory::GetForBrowserContext(
      Profile::FromBrowserContext(testing_profile_.get()))
      ->MaybeRefreshOnStartUp();

  // Incognito profile should share the advanced protection status with the
  // original profile.
  EXPECT_FALSE(AdvancedProtectionStatusManager::IsUnderAdvancedProtection(
      testing_profile_->GetOffTheRecordProfile()));
  EXPECT_FALSE(AdvancedProtectionStatusManager::IsUnderAdvancedProtection(
      testing_profile_.get()));
}

TEST_F(AdvancedProtectionStatusManagerTest, StayInAdvancedProtection) {
  base::Time last_update = base::Time::Now();
  testing_profile_->GetPrefs()->SetInt64(
      prefs::kAdvancedProtectionLastRefreshInUs,
      last_update.ToDeltaSinceWindowsEpoch().InMicroseconds());

  std::string account_id =
      SignIn("test@test.com", /* is_under_advanced_protection = */ true);
  AdvancedProtectionStatusManager aps_manager(
      testing_profile_.get(), base::TimeDelta() /*no min delay*/);
  ASSERT_FALSE(aps_manager.GetPrimaryAccountId().empty());
  ASSERT_TRUE(aps_manager.is_under_advanced_protection());

  // Simulate gets refresh token.
  aps_manager.OnGetIDToken(account_id, kIdTokenAdvancedProtectionEnabled);
  EXPECT_GT(
      base::Time::FromDeltaSinceWindowsEpoch(base::TimeDelta::FromMicroseconds(
          testing_profile_->GetPrefs()->GetInt64(
              prefs::kAdvancedProtectionLastRefreshInUs))),
      last_update);
  EXPECT_TRUE(aps_manager.IsRefreshScheduled());
  aps_manager.UnsubscribeFromSigninEvents();
}

#if !defined(OS_CHROMEOS)
// Not applicable to Chrome OS.
TEST_F(AdvancedProtectionStatusManagerTest, SignInAndSignOutEvent) {
  AdvancedProtectionStatusManager aps_manager(
      testing_profile_.get(), base::TimeDelta() /*no min delay*/);
  ASSERT_FALSE(aps_manager.is_under_advanced_protection());
  ASSERT_TRUE(aps_manager.GetPrimaryAccountId().empty());

  SignIn("test@test.com", /* is_under_advanced_protection = */ true);
  EXPECT_TRUE(aps_manager.is_under_advanced_protection());
  EXPECT_TRUE(aps_manager.IsRefreshScheduled());

  identity_test_env()->ClearPrimaryAccount();
  EXPECT_FALSE(aps_manager.is_under_advanced_protection());
  EXPECT_TRUE(testing_profile_->GetPrefs()->HasPrefPath(
      prefs::kAdvancedProtectionLastRefreshInUs));
  EXPECT_FALSE(aps_manager.IsRefreshScheduled());
  aps_manager.UnsubscribeFromSigninEvents();
}
#endif

TEST_F(AdvancedProtectionStatusManagerTest, AccountRemoval) {
  AdvancedProtectionStatusManager aps_manager(
      testing_profile_.get(), base::TimeDelta() /*no min delay*/);
  ASSERT_FALSE(aps_manager.is_under_advanced_protection());
  ASSERT_TRUE(aps_manager.GetPrimaryAccountId().empty());

  std::string account_id =
      SignIn("test@test.com", /* is_under_advanced_protection = */ false);
  EXPECT_FALSE(aps_manager.is_under_advanced_protection());
  EXPECT_FALSE(aps_manager.IsRefreshScheduled());

  // Simulates account update.
  identity_test_env()
      ->identity_manager()
      ->GetAccountsMutator()
      ->UpdateAccountInfo(account_id,
                          /*is_child_account=*/false,
                          /*is_under_advanced_protection=*/true);
  EXPECT_TRUE(aps_manager.is_under_advanced_protection());
  EXPECT_TRUE(aps_manager.IsRefreshScheduled());

  // This call is necessary to ensure that the account removal is fully
  // processed in this testing context.
  identity_test_env()->EnableRemovalOfExtendedAccountInfo();
  identity_test_env()->identity_manager()->GetAccountsMutator()->RemoveAccount(
      account_id,
      signin_metrics::SourceForRefreshTokenOperation::kUserMenu_RemoveAccount);
  EXPECT_FALSE(aps_manager.is_under_advanced_protection());
  EXPECT_TRUE(testing_profile_->GetPrefs()->HasPrefPath(
      prefs::kAdvancedProtectionLastRefreshInUs));
  EXPECT_FALSE(aps_manager.IsRefreshScheduled());
  aps_manager.UnsubscribeFromSigninEvents();
}

TEST_F(AdvancedProtectionStatusManagerTest,
       AdvancedProtectionDisabledAfterSignin) {
  AdvancedProtectionStatusManager aps_manager(
      testing_profile_.get(), base::TimeDelta() /*no min delay*/);
  // There is no account, so the timer should not run at startup.
  EXPECT_FALSE(aps_manager.IsRefreshScheduled());

  std::string account_id =
      SignIn("test@test.com", /* is_under_advanced_protection = */ true);

  // Now that we've signed into Advanced Protection, we should have a scheduled
  // refresh.
  EXPECT_TRUE(aps_manager.is_under_advanced_protection());
  EXPECT_TRUE(aps_manager.IsRefreshScheduled());

  // Skip the 24 hour wait, and try to refresh the token now.
  aps_manager.timer_.FireNow();
  MakeOAuthTokenFetchSucceed(account_id,
                             /* is_under_advanced_protection = */ false);

  EXPECT_FALSE(aps_manager.is_under_advanced_protection());
  EXPECT_FALSE(aps_manager.IsRefreshScheduled());

  aps_manager.UnsubscribeFromSigninEvents();
}

TEST_F(AdvancedProtectionStatusManagerTest,
       StartupAfterLongWaitRefreshesImmediately) {
  std::string account_id =
      SignIn("test@test.com", /* is_under_advanced_protection = */ true);
  base::RunLoop().RunUntilIdle();

  base::Time last_refresh_time =
      base::Time::Now() - base::TimeDelta::FromDays(1);
  testing_profile_->GetPrefs()->SetInt64(
      prefs::kAdvancedProtectionLastRefreshInUs,
      last_refresh_time.ToDeltaSinceWindowsEpoch().InMicroseconds());

  AdvancedProtectionStatusManager aps_manager(
      testing_profile_.get(), base::TimeDelta() /*no min delay*/);
  ASSERT_FALSE(aps_manager.GetPrimaryAccountId().empty());
  ASSERT_TRUE(aps_manager.is_under_advanced_protection());
  EXPECT_TRUE(aps_manager.IsRefreshScheduled());

  MakeOAuthTokenFetchSucceed(account_id,
                             /* is_under_advanced_protection = */ false);

  EXPECT_FALSE(aps_manager.is_under_advanced_protection());
  EXPECT_FALSE(aps_manager.IsRefreshScheduled());

  aps_manager.UnsubscribeFromSigninEvents();
}

}  // namespace safe_browsing
