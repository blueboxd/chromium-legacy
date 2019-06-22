// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_reporting_default_state.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/unified_consent/feature.h"
#import "ios/chrome/app/main_controller.h"
#include "ios/chrome/browser/application_context.h"
#import "ios/chrome/browser/geolocation/omnibox_geolocation_controller+Testing.h"
#import "ios/chrome/browser/geolocation/omnibox_geolocation_controller.h"
#import "ios/chrome/browser/geolocation/test_location_manager.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui.h"
#import "ios/chrome/browser/ui/authentication/signin_earlgrey_utils.h"
#import "ios/chrome/browser/ui/first_run/first_run_chrome_signin_view_controller.h"
#include "ios/chrome/browser/ui/first_run/welcome_to_chrome_view_controller.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::AccountConsistencySetupSigninButton;
using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsMenuBackButton;
using chrome_test_util::SyncSettingsConfirmButton;

namespace {

// Returns matcher for the opt in accept button.
id<GREYMatcher> FirstRunOptInAcceptButton() {
  return ButtonWithAccessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_FIRSTRUN_OPT_IN_ACCEPT_BUTTON));
}

// Returns matcher for the skip sign in button.
id<GREYMatcher> SkipSigninButton() {
  return grey_accessibilityID(kSignInSkipButtonAccessibilityIdentifier);
}

// Returns matcher for the first run account consistency skip button.
id<GREYMatcher> FirstRunAccountConsistencySkipButton() {
  return ButtonWithAccessibilityLabelId(
      IDS_IOS_FIRSTRUN_ACCOUNT_CONSISTENCY_SKIP_BUTTON);
}

// Returns matcher for the undo sign in button.
id<GREYMatcher> UndoAccountConsistencyButton() {
  return ButtonWithAccessibilityLabelId(
      IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_UNDO_BUTTON);
}
}

@interface MainController (ExposedForTesting)
- (void)showFirstRunUI;
@end

// Tests first run settings and navigation.
@interface FirstRunTestCase : ChromeTestCase
@end

@implementation FirstRunTestCase

- (void)setUp {
  [super setUp];

  BooleanPrefMember metricsEnabledPref;
  metricsEnabledPref.Init(metrics::prefs::kMetricsReportingEnabled,
                          GetApplicationContext()->GetLocalState());
  metricsEnabledPref.SetValue(NO);
  IntegerPrefMember defaultOptInPref;
  defaultOptInPref.Init(metrics::prefs::kMetricsDefaultOptIn,
                        GetApplicationContext()->GetLocalState());
  defaultOptInPref.SetValue(metrics::EnableMetricsDefault::DEFAULT_UNKNOWN);

  TestLocationManager* locationManager = [[TestLocationManager alloc] init];
  [locationManager setLocationServicesEnabled:NO];
  [[OmniboxGeolocationController sharedInstance]
      setLocationManager:locationManager];
}

+ (void)tearDown {
  IntegerPrefMember defaultOptInPref;
  defaultOptInPref.Init(metrics::prefs::kMetricsDefaultOptIn,
                        GetApplicationContext()->GetLocalState());
  defaultOptInPref.SetValue(metrics::EnableMetricsDefault::DEFAULT_UNKNOWN);

  [[OmniboxGeolocationController sharedInstance] setLocationManager:nil];

  [super tearDown];
}

// Navigates to the terms of service and back.
- (void)testPrivacy {
  [chrome_test_util::GetMainController() showFirstRunUI];

  id<GREYMatcher> privacyLink = grey_accessibilityLabel(@"Privacy Notice");
  [[EarlGrey selectElementWithMatcher:privacyLink] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_FIRSTRUN_PRIVACY_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"ic_arrow_back"),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitButton),
                                   nil)] performAction:grey_tap()];

  // Ensure we went back to the First Run screen.
  [[EarlGrey selectElementWithMatcher:privacyLink]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Navigates to the terms of service and back.
- (void)testTermsAndConditions {
  [chrome_test_util::GetMainController() showFirstRunUI];

  id<GREYMatcher> termsOfServiceLink =
      grey_accessibilityLabel(@"Terms of Service");
  [[EarlGrey selectElementWithMatcher:termsOfServiceLink]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_FIRSTRUN_TERMS_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"ic_arrow_back"),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitButton),
                                   nil)] performAction:grey_tap()];

  // Ensure we went back to the First Run screen.
  [[EarlGrey selectElementWithMatcher:termsOfServiceLink]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Toggle the UMA checkbox.
- (void)testToggleMetricsOn {
  [chrome_test_util::GetMainController() showFirstRunUI];

  id<GREYMatcher> metrics =
      grey_accessibilityID(kUMAMetricsButtonAccessibilityIdentifier);
  [[EarlGrey selectElementWithMatcher:metrics] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:FirstRunOptInAcceptButton()]
      performAction:grey_tap()];

  BOOL metricsOptIn = GetApplicationContext()->GetLocalState()->GetBoolean(
      metrics::prefs::kMetricsReportingEnabled);
  GREYAssert(
      metricsOptIn != [WelcomeToChromeViewController defaultStatsCheckboxValue],
      @"Metrics reporting pref is incorrect.");
}

// Dismisses the first run screens.
- (void)testDismissFirstRun {
  [chrome_test_util::GetMainController() showFirstRunUI];

  [[EarlGrey selectElementWithMatcher:FirstRunOptInAcceptButton()]
      performAction:grey_tap()];

  PrefService* preferences = GetApplicationContext()->GetLocalState();
  GREYAssert(
      preferences->GetBoolean(metrics::prefs::kMetricsReportingEnabled) ==
          [WelcomeToChromeViewController defaultStatsCheckboxValue],
      @"Metrics reporting does not match.");

  [[EarlGrey selectElementWithMatcher:SkipSigninButton()]
      performAction:grey_tap()];

  id<GREYMatcher> newTab =
      grey_kindOfClass(NSClassFromString(@"ContentSuggestionsHeaderView"));
  [[EarlGrey selectElementWithMatcher:newTab]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Signs in to an account and then taps the Undo button to sign out.
- (void)testSignInAndUndo {
  if (unified_consent::IsUnifiedConsentFeatureEnabled()) {
    LOG(WARNING) << "Skipping test as there is no undo operation when "
                    "Unified Consent is enabled.";
    return;
  }

  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  // Launch First Run and accept tems of services.
  [chrome_test_util::GetMainController() showFirstRunUI];
  [[EarlGrey selectElementWithMatcher:FirstRunOptInAcceptButton()]
      performAction:grey_tap()];

  // Sign In |identity|.
  [[EarlGrey selectElementWithMatcher:AccountConsistencySetupSigninButton()]
      performAction:grey_tap()];

  [SigninEarlGreyUtils checkSignedInWithIdentity:identity];

  // Undo the sign-in and dismiss the Sign In screen.
  [[EarlGrey selectElementWithMatcher:UndoAccountConsistencyButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:FirstRunAccountConsistencySkipButton()]
      performAction:grey_tap()];

  // |identity| shouldn't be signed in.
  [SigninEarlGreyUtils checkSignedOut];
}

// Signs in to an account and then taps the Advanced link to go to settings.
- (void)testSignInAndTapSettingsLink {
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  // Launch First Run and accept tems of services.
  [chrome_test_util::GetMainController() showFirstRunUI];
  [[EarlGrey selectElementWithMatcher:FirstRunOptInAcceptButton()]
      performAction:grey_tap()];

  if (!unified_consent::IsUnifiedConsentFeatureEnabled()) {
    // Sign In |identity|.
    [[EarlGrey selectElementWithMatcher:AccountConsistencySetupSigninButton()]
        performAction:grey_tap()];

    [SigninEarlGreyUtils checkSignedInWithIdentity:identity];
  }

  // Tap Settings link.
  [SigninEarlGreyUI tapSettingsLink];

  // Check Sync hasn't started yet, allowing the user to change some settings.
  SyncSetupService* sync_service = SyncSetupServiceFactory::GetForBrowserState(
      chrome_test_util::GetOriginalBrowserState());
  GREYAssertFalse(sync_service->HasFinishedInitialSetup(),
                  @"Sync shouldn't have finished its original setup yet");

  // Close Settings, user is still signed in and sync is now starting.
  if (unified_consent::IsUnifiedConsentFeatureEnabled()) {
    [[EarlGrey selectElementWithMatcher:SyncSettingsConfirmButton()]
        performAction:grey_tap()];
  } else {
    [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
        performAction:grey_tap()];
  }

  [SigninEarlGreyUtils checkSignedInWithIdentity:identity];

  GREYAssertTrue(sync_service->HasFinishedInitialSetup(),
                 @"Sync should have finished its original setup");
}

@end
