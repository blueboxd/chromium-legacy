// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ios/ios_util.h"
#include "base/macros.h"
#include "base/stl_util.h"
#import "base/test/ios/wait_util.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/metrics/metrics_app_interface.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui.h"
#import "ios/chrome/browser/ui/authentication/signin_earlgrey_utils.h"
#import "ios/chrome/browser/ui/authentication/signin_earlgrey_utils_app_interface.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::AccountsSyncButton;
using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::ClearBrowsingDataButton;
using chrome_test_util::ClearBrowsingDataCell;
using chrome_test_util::ConfirmClearBrowsingDataButton;
using chrome_test_util::GoogleServicesSettingsButton;
using chrome_test_util::SettingsAccountButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsMenuPrivacyButton;
using chrome_test_util::SyncSwitchCell;
using chrome_test_util::TurnSyncSwitchOn;

namespace {

void ClearBrowsingData() {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsMenuPrivacyButton()];
  [ChromeEarlGreyUI tapPrivacyMenuButton:ClearBrowsingDataCell()];
  [ChromeEarlGreyUI tapClearBrowsingDataMenuButton:ClearBrowsingDataButton()];
  [[EarlGrey selectElementWithMatcher:ConfirmClearBrowsingDataButton()]
      performAction:grey_tap()];

  // Wait until activity indicator modal is cleared, meaning clearing browsing
  // data has been finished.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

void OpenNewIncognitoTab() {
  NSUInteger incognito_tab_count = [ChromeEarlGrey incognitoTabCount];
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey waitForIncognitoTabCount:(incognito_tab_count + 1)];
  GREYAssert([ChromeEarlGrey isIncognitoMode],
             @"Failed to switch to incognito mode.");
}

void CloseCurrentIncognitoTab() {
  NSUInteger incognito_tab_count = [ChromeEarlGrey incognitoTabCount];
  [ChromeEarlGrey closeCurrentTab];
  [ChromeEarlGrey waitForIncognitoTabCount:(incognito_tab_count - 1)];
}

void CloseAllIncognitoTabs() {
  [ChromeEarlGrey closeAllIncognitoTabs];
  [ChromeEarlGrey waitForIncognitoTabCount:0];

  // The user is dropped into the tab grid after closing the last incognito tab.
  // Therefore this test must manually switch back to showing the normal tabs.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridOpenTabsPanelButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];
  GREYAssert(![ChromeEarlGrey isIncognitoMode],
             @"Failed to switch to normal mode.");
}

void OpenNewRegularTab() {
  NSUInteger tab_count = [ChromeEarlGrey mainTabCount];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey waitForMainTabCount:(tab_count + 1)];
}

}  // namespace

// UKM tests.
@interface UKMTestCase : ChromeTestCase

@end

@implementation UKMTestCase

#if defined(CHROME_EARL_GREY_2)
+ (void)setUpForTestCase {
  [super setUpForTestCase];
  [self setUpHelper];
}
#elif defined(CHROME_EARL_GREY_1)
+ (void)setUp {
  [super setUp];
  [self setUpHelper];
}
#else
#error Not an EarlGrey Test
#endif

+ (void)setUpHelper {
  if (![ChromeEarlGrey isUKMEnabled]) {
    // ukm::kUkmFeature feature is not enabled. You need to pass
    // --enable-features=Ukm command line argument in order to run this test.
    DCHECK(false);
  }
}

- (void)setUp {
  [super setUp];

  [ChromeEarlGrey waitForSyncInitialized:NO
                             syncTimeout:syncher::kSyncUKMOperationsTimeout];
  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:NO],
             @"Failed to assert that UKM was not enabled.");
  // Sign in to Chrome and turn sync on.
  //
  // Note: URL-keyed anonymized data collection is turned on as part of the
  // flow to Sign in to Chrome and Turn sync on. This matches the main user
  // flow that enables UKM.
  [SigninEarlGreyUI signinWithFakeIdentity:[SigninEarlGreyUtils fakeIdentity1]];
  [ChromeEarlGrey waitForSyncInitialized:YES
                             syncTimeout:syncher::kSyncUKMOperationsTimeout];

  // Grant metrics consent and update MetricsServicesManager.
  [MetricsAppInterface overrideMetricsAndCrashReportingForTesting];
  GREYAssert(![MetricsAppInterface setMetricsAndCrashReportingForTesting:YES],
             @"Unpaired set/reset of user consent.");
  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:YES],
             @"Failed to assert that UKM was enabled.");
}

- (void)tearDown {
  [ChromeEarlGrey waitForSyncInitialized:YES
                             syncTimeout:syncher::kSyncUKMOperationsTimeout];
  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:YES],
             @"Failed to assert that UKM was enabled.");

  // Revoke metrics consent and update MetricsServicesManager.
  GREYAssert([MetricsAppInterface setMetricsAndCrashReportingForTesting:NO],
             @"Unpaired set/reset of user consent.");
  [MetricsAppInterface stopOverridingMetricsAndCrashReportingForTesting];
  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:NO],
             @"Failed to assert that UKM was not enabled.");

  // Sign out of Chrome and Turn off sync.
  //
  // Note: URL-keyed anonymized data collection is turned off as part of the
  // flow to Sign out of Chrome and Turn sync off. This matches the main user
  // flow that disables UKM.
  if (![SigninEarlGreyUtilsAppInterface isSignedOut]) {
    [SigninEarlGreyUtilsAppInterface signOut];
  }

  [ChromeEarlGrey waitForSyncInitialized:NO
                             syncTimeout:syncher::kSyncUKMOperationsTimeout];
  [ChromeEarlGrey clearSyncServerData];

  [super tearDown];
}

// The tests in this file should correspond with the ones in
// //chrome/browser/metrics/ukm_browsertest.cc

// Make sure that UKM is disabled while an incognito tab is open.
#if defined(CHROME_EARL_GREY_1)
// TODO(crbug.com/1033726): EG1 Test fails on iOS 12.
#define MAYBE_testRegularPlusIncognito DISABLED_testRegularPlusIncognito
#else
#define MAYBE_testRegularPlusIncognito testRegularPlusIncognito
#endif
- (void)MAYBE_testRegularPlusIncognito {
  uint64_t originalClientID = [MetricsAppInterface UKMClientID];

  OpenNewIncognitoTab();
  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:NO],
             @"Failed to assert that UKM was not enabled.");

  // Opening another regular tab mustn't enable UKM.
  OpenNewRegularTab();
  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:NO],
             @"Failed to assert that UKM was not enabled.");

  // Opening and closing an incognito tab mustn't enable UKM.
  OpenNewIncognitoTab();
  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:NO],
             @"Failed to assert that UKM was not enabled.");
  CloseCurrentIncognitoTab();
  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:NO],
             @"Failed to assert that UKM was not enabled.");

  CloseAllIncognitoTabs();
  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:YES],
             @"Failed to assert that UKM was enabled.");

  // Client ID should not have been reset.
  GREYAssertEqual(originalClientID, [MetricsAppInterface UKMClientID],
                  @"Client ID was reset.");
}

// Make sure opening a real tab after Incognito doesn't enable UKM.
#if defined(CHROME_EARL_GREY_1)
// TODO(crbug.com/1033726): EG1 Test fails on iOS 12.
#define MAYBE_testIncognitoPlusRegular DISABLED_testIncognitoPlusRegular
#else
#define MAYBE_testIncognitoPlusRegular testIncognitoPlusRegular
#endif
- (void)MAYBE_testIncognitoPlusRegular {
  uint64_t originalClientID = [MetricsAppInterface UKMClientID];
  [ChromeEarlGrey closeAllTabs];
  [ChromeEarlGrey waitForMainTabCount:0];

  OpenNewIncognitoTab();
  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:NO],
             @"Failed to assert that UKM was not enabled.");

  // Opening another regular tab mustn't enable UKM.
  OpenNewRegularTab();
  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:NO],
             @"Failed to assert that UKM was not enabled.");

  [ChromeEarlGrey closeAllIncognitoTabs];
  [ChromeEarlGrey waitForIncognitoTabCount:0];
  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:YES],
             @"Failed to assert that UKM was enabled.");

  // Client ID should not have been reset.
  GREYAssertEqual(originalClientID, [MetricsAppInterface UKMClientID],
                  @"Client ID was reset.");
}

// testOpenNonSync not needed, since there can't be multiple profiles.

// Make sure that UKM is disabled when metrics consent is revoked.
- (void)testMetricsConsent {
#if defined(CHROME_EARL_GREY_1)
  // TODO(crbug.com/1033726): EG1 Test fails on iOS 12.
  if (!base::ios::IsRunningOnIOS13OrLater()) {
    EARL_GREY_TEST_DISABLED(@"EG1 Fails on iOS 12.");
  }
#endif

  uint64_t originalClientID = [MetricsAppInterface UKMClientID];

  [MetricsAppInterface setMetricsAndCrashReportingForTesting:NO];

  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:NO],
             @"Failed to assert that UKM was not enabled.");

  [MetricsAppInterface setMetricsAndCrashReportingForTesting:YES];

  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:YES],
             @"Failed to assert that UKM was enabled.");
  // Client ID should have been reset.
  GREYAssertNotEqual(originalClientID, [MetricsAppInterface UKMClientID],
                     @"Client ID was not reset.");
}

// Make sure that providing metrics consent doesn't enable UKM w/o sync.
- (void)testConsentAddedButNoSync {
  [SigninEarlGreyUtilsAppInterface signOut];
  [MetricsAppInterface setMetricsAndCrashReportingForTesting:NO];
  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:NO],
             @"Failed to assert that UKM was not enabled.");

  [MetricsAppInterface setMetricsAndCrashReportingForTesting:YES];
  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:NO],
             @"Failed to assert that UKM was not enabled.");

  [SigninEarlGreyUI signinWithFakeIdentity:[SigninEarlGreyUtils fakeIdentity1]];
  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:YES],
             @"Failed to assert that UKM was enabled.");
}

// Make sure that UKM is disabled when sync is disabled.
- (void)testSingleDisableSync {
#if defined(CHROME_EARL_GREY_1)
  // TODO(crbug.com/1033726): EG1 Test fails on iOS 12.
  if (!base::ios::IsRunningOnIOS13OrLater()) {
    EARL_GREY_TEST_DISABLED(@"EG1 Fails on iOS 12.");
  }
#endif

  uint64_t originalClientID = [MetricsAppInterface UKMClientID];

  [ChromeEarlGreyUI openSettingsMenu];
    // Open Sync and Google services settings
    [ChromeEarlGreyUI tapSettingsMenuButton:GoogleServicesSettingsButton()];
    // Toggle "Make searches and browsing better" switch off.

    [[[EarlGrey
        selectElementWithMatcher:chrome_test_util::SettingsSwitchCell(
                                     @"betterSearchAndBrowsingItem_switch",
                                     YES)]
           usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
        onElementWithMatcher:chrome_test_util::GoogleServicesSettingsView()]
        performAction:chrome_test_util::TurnSettingsSwitchOn(NO)];

    GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:NO],
               @"Failed to assert that UKM was not enabled.");

    // Toggle "Make searches and browsing better" switch on.
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::SettingsSwitchCell(
                                     @"betterSearchAndBrowsingItem_switch", NO)]
        performAction:chrome_test_util::TurnSettingsSwitchOn(YES)];

    GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:YES],
               @"Failed to assert that UKM was enabled.");
    // Client ID should have been reset.
    GREYAssertNotEqual(originalClientID, [MetricsAppInterface UKMClientID],
                       @"Client ID was not reset.");

    [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
        performAction:grey_tap()];
}

// testMultiDisableSync not needed, since there can't be multiple profiles.

// Make sure that UKM is disabled when sync is not enabled.
- (void)testSingleSyncSignout {
#if defined(CHROME_EARL_GREY_1)
  // TODO(crbug.com/1033726): EG1 Test fails on iOS 12.
  if (!base::ios::IsRunningOnIOS13OrLater()) {
    EARL_GREY_TEST_DISABLED(@"EG1 Fails on iOS 12.");
  }
#endif
  uint64_t originalClientID = [MetricsAppInterface UKMClientID];

  [SigninEarlGreyUtilsAppInterface signOut];

  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:NO],
             @"Failed to assert that UKM was not enabled.");
  // Client ID should have been reset by signout.
  GREYAssertNotEqual(originalClientID, [MetricsAppInterface UKMClientID],
                     @"Client ID was not reset.");

  originalClientID = [MetricsAppInterface UKMClientID];
  [SigninEarlGreyUI signinWithFakeIdentity:[SigninEarlGreyUtils fakeIdentity1]];

  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:YES],
             @"Failed to assert that UKM was enabled.");
  // Client ID should not have been reset.
  GREYAssertEqual(originalClientID, [MetricsAppInterface UKMClientID],
                  @"Client ID was reset.");
}

// testMultiSyncSignout not needed, since there can't be multiple profiles.

// testMetricsReporting not needed, since iOS doesn't use sampling.

// TODO(crbug.com/866598): Re-enable this test.
- (void)DISABLED_testHistoryDelete {
  uint64_t originalClientID = [MetricsAppInterface UKMClientID];

  const uint64_t kDummySourceId = 0x54321;
  [MetricsAppInterface UKMRecordDummySource:kDummySourceId];
  GREYAssert([MetricsAppInterface UKMHasDummySource:kDummySourceId],
             @"Dummy source failed to record.");

  ClearBrowsingData();

  // Other sources may already have been recorded since the data was cleared,
  // but the dummy source should be gone.
  GREYAssert(![MetricsAppInterface UKMHasDummySource:kDummySourceId],
             @"Dummy source was not purged.");
  GREYAssertEqual(originalClientID, [MetricsAppInterface UKMClientID],
                  @"Client ID was reset.");
  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:YES],
             @"Failed to assert that UKM was enabled.");
}

@end
