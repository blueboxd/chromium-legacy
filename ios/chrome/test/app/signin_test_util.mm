// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/test/app/signin_test_util.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#import "base/test/ios/wait_util.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "google_apis/gaia/gaia_constants.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/chrome/browser/signin/gaia_auth_fetcher_ios.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace chrome_test_util {

namespace {

// Forgets all identities from the ChromeIdentity services. Returns true if all
// identities were successfully forgotten.
bool ForgetAllIdentities() {
  ios::ChromeIdentityService* identity_service =
      ios::GetChromeBrowserProvider()->GetChromeIdentityService();

  if (!identity_service->HasIdentities())
    return YES;

  NSArray* identities_to_remove =
      [NSArray arrayWithArray:identity_service->GetAllIdentities()];
  NSMutableArray* identities_left =
      [NSMutableArray arrayWithArray:identities_to_remove];

  for (ChromeIdentity* identity in identities_to_remove) {
    identity_service->ForgetIdentity(identity, ^(NSError* error) {
      if (error) {
        NSLog(@"ForgetIdentity failed: [identity = %@, error = %@]",
              identity.userEmail, [error localizedDescription]);
      }
      [identities_left removeObject:identity];
    });
  }

  // Wait maximum 10s for all forget identitites to be forgotten.
  NSDate* deadline = [NSDate dateWithTimeIntervalSinceNow:10.0];
  while ([identities_left count] &&
         [[NSDate date] compare:deadline] != NSOrderedDescending) {
    base::test::ios::SpinRunLoopWithMaxDelay(
        base::TimeDelta::FromSecondsD(0.01));
  }

  return !identity_service->HasIdentities();
}

}  // namespace

void SetUpMockAuthentication() {
  ios::ChromeBrowserProvider* provider = ios::GetChromeBrowserProvider();
  std::unique_ptr<ios::FakeChromeIdentityService> service(
      new ios::FakeChromeIdentityService());
  service->SetUpForIntegrationTests();
  provider->SetChromeIdentityServiceForTesting(std::move(service));
  AuthenticationServiceFactory::GetForBrowserState(GetOriginalBrowserState())
      ->ResetChromeIdentityServiceObserverForTesting();
}

void TearDownMockAuthentication() {
  ios::ChromeBrowserProvider* provider = ios::GetChromeBrowserProvider();
  provider->SetChromeIdentityServiceForTesting(nullptr);
  AuthenticationServiceFactory::GetForBrowserState(GetOriginalBrowserState())
      ->ResetChromeIdentityServiceObserverForTesting();
}

void SetUpMockAccountReconcilor() {
  GaiaAuthFetcherIOS::SetShouldUseGaiaAuthFetcherIOSForTesting(false);
}

void TearDownMockAccountReconcilor() {
  GaiaAuthFetcherIOS::SetShouldUseGaiaAuthFetcherIOSForTesting(true);
}

bool SignOutAndClearAccounts() {
  // EarlGrey monitors network requests by swizzling internal iOS network
  // objects and expects them to be dealloced before the tear down. It is
  // important to autorelease all objects that make network requests to avoid
  // EarlGrey being confused about on-going network traffic..
  @autoreleasepool {
    ios::ChromeBrowserState* browser_state = GetOriginalBrowserState();
    DCHECK(browser_state);

    // Sign out current user.
    AuthenticationService* authentication_service =
        AuthenticationServiceFactory::GetForBrowserState(browser_state);
    if (authentication_service->IsAuthenticated()) {
      authentication_service->SignOut(signin_metrics::SIGNOUT_TEST, nil);
    }

    // Clear last signed in user preference.
    browser_state->GetPrefs()->ClearPref(prefs::kGoogleServicesLastAccountId);
    browser_state->GetPrefs()->ClearPref(prefs::kGoogleServicesLastUsername);

    // |SignOutAndClearAccounts()| is called during shutdown. Commit all pref
    // changes to ensure that clearing the last signed in account is saved on
    // disk in case Chrome crashes during shutdown.
    browser_state->GetPrefs()->CommitPendingWrite();

    return ForgetAllIdentities();
  }
}

void ResetMockAuthentication() {
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->SetFakeMDMError(false);
}

void ResetSigninPromoPreferences() {
  ios::ChromeBrowserState* browser_state = GetOriginalBrowserState();
  PrefService* prefs = browser_state->GetPrefs();
  prefs->SetInteger(prefs::kIosBookmarkSigninPromoDisplayedCount, 0);
  prefs->SetBoolean(prefs::kIosBookmarkPromoAlreadySeen, false);
  prefs->SetInteger(prefs::kIosSettingsSigninPromoDisplayedCount, 0);
  prefs->SetBoolean(prefs::kIosSettingsPromoAlreadySeen, false);
}

}  // namespace chrome_test_util
