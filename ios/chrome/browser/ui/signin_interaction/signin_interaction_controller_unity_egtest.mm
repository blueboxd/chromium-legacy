// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include "base/auto_reset.h"
#import "base/test/ios/wait_util.h"
#include "components/unified_consent/feature.h"
#import "ios/chrome/browser/ui/authentication/chrome_signin_view_controller.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui.h"
#import "ios/chrome/browser/ui/authentication/signin_earlgrey_utils.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_cell.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/signin_interaction/signin_interaction_controller_egtest_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_interaction_manager.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#import "ios/testing/earl_grey/matchers.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::BookmarksNavigationBarDoneButton;
using chrome_test_util::PrimarySignInButton;
using chrome_test_util::SecondarySignInButton;
using chrome_test_util::SettingsDoneButton;

namespace {

// Returns a matcher for |userEmail| in IdentityChooserViewController.
id<GREYMatcher> identityChooserButtonMatcherWithEmail(NSString* userEmail) {
  return grey_allOf(grey_accessibilityID(userEmail),
                    grey_kindOfClass([IdentityChooserCell class]),
                    grey_sufficientlyVisible(), nil);
}

// Opens Accounts Settings and tap the sign out button. Assumes that the main
// settings page is visible.
void SignOutFromSettings() {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsAccountButton()]
      performAction:grey_tap()];

  const CGFloat scroll_displacement = 100.0;
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   chrome_test_util::SignOutAccountsButton(),
                                   grey_interactable(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  scroll_displacement)
      onElementWithMatcher:chrome_test_util::SettingsAccountsCollectionView()]
      performAction:grey_tap()];
  TapButtonWithLabelId(IDS_IOS_DISCONNECT_DIALOG_CONTINUE_BUTTON_MOBILE);
  [SigninEarlGreyUtils checkSignedOut];
}

}  // namespace

// Sign-in interactions tests that require Unified Consent to be enabled.
@interface SigninInteractionControllerUnityEnabledTestCase : ChromeTestCase
@end

@implementation SigninInteractionControllerUnityEnabledTestCase

- (void)setUp {
  [super setUp];

  CHECK(unified_consent::IsUnifiedConsentFeatureEnabled())
      << "This test suite must be run with Unified Consent feature enabled.";
}

// Tests the "ADD ACCOUNT" button in the identity chooser view controller.
- (void)testAddAccountAutomatically {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:PrimarySignInButton()];
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  // Tap on "ADD ACCOUNT".
  [SigninEarlGreyUI tapAddAccountButton];
  // Check for the fake SSO screen.
  WaitForMatcher(grey_accessibilityID(kFakeAddAccountViewIdentifier));
  // Close the SSO view controller.
  id<GREYMatcher> matcher =
      grey_allOf(chrome_test_util::ButtonWithAccessibilityLabel(@"Cancel"),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];
  // Make sure the SSO view controller is fully removed before ending the test.
  // The tear down needs to remove other view controllers, and it cannot be done
  // during the animation of the SSO view controler.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

// Tests to remove the last identity in the identity chooser.
- (void)testRemoveLastAccount {
  // Set up a fake identity.
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  // Open the identity chooser.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];
  WaitForMatcher(identityChooserButtonMatcherWithEmail(identity.userEmail));

  // Remove the fake identity.
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->RemoveIdentity(identity);
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  // Check that the identity has been removed.
  [[EarlGrey selectElementWithMatcher:identityChooserButtonMatcherWithEmail(
                                          identity.userEmail)]
      assertWithMatcher:grey_notVisible()];
}

// Tests signing in with one account, switching sync account to a second and
// choosing to keep the browsing data separate during the switch.
- (void)testSignInSwitchAccountsAndKeepDataSeparate {
  // The ChromeSigninView's activity indicator must be hidden as the import
  // data UI is presented on top of the activity indicator and Earl Grey cannot
  // interact with any UI while an animation is active.
  std::unique_ptr<base::AutoReset<BOOL>> hideActivityMonitor =
      [ChromeSigninViewController hideActivityIndicatorForTesting];

  // Set up the fake identities.
  ios::FakeChromeIdentityService* identity_service =
      ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
  ChromeIdentity* identity1 = [SigninEarlGreyUtils fakeIdentity1];
  ChromeIdentity* identity2 = [SigninEarlGreyUtils fakeIdentity2];
  identity_service->AddIdentity(identity1);
  identity_service->AddIdentity(identity2);

  [SigninEarlGreyUI signinWithIdentity:identity1];
  [ChromeEarlGreyUI openSettingsMenu];
  SignOutFromSettings();

  // Sign in with |identity2|.
  [[EarlGrey selectElementWithMatcher:SecondarySignInButton()]
      performAction:grey_tap()];
  [SigninEarlGreyUI selectIdentityWithEmail:identity2.userEmail];
  [SigninEarlGreyUI confirmSigninConfirmationDialog];

  // Switch Sync account to |identity2| should ask whether date should be
  // imported or kept separate. Choose to keep data separate.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::SettingsImportDataKeepSeparateButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SettingsImportDataContinueButton()]
      performAction:grey_tap()];

  // Check the signed-in user did change.
  [SigninEarlGreyUtils checkSignedInWithIdentity:identity2];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests signing in with one account, switching sync account to a second and
// choosing to import the browsing data during the switch.
- (void)testSignInSwitchAccountsAndImportData {
  // The ChromeSigninView's activity indicator must be hidden as the import
  // data UI is presented on top of the activity indicator and Earl Grey cannot
  // interact with any UI while an animation is active.
  std::unique_ptr<base::AutoReset<BOOL>> hideActivityMonitor =
      [ChromeSigninViewController hideActivityIndicatorForTesting];

  // Set up the fake identities.
  ios::FakeChromeIdentityService* identity_service =
      ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
  ChromeIdentity* identity1 = [SigninEarlGreyUtils fakeIdentity1];
  ChromeIdentity* identity2 = [SigninEarlGreyUtils fakeIdentity2];
  identity_service->AddIdentity(identity1);
  identity_service->AddIdentity(identity2);

  // Sign in to |identity1|.
  [SigninEarlGreyUI signinWithIdentity:identity1];
  [ChromeEarlGreyUI openSettingsMenu];
  SignOutFromSettings();

  // Sign in with |identity2|.
  [[EarlGrey selectElementWithMatcher:SecondarySignInButton()]
      performAction:grey_tap()];
  [SigninEarlGreyUI selectIdentityWithEmail:identity2.userEmail];
  [SigninEarlGreyUI confirmSigninConfirmationDialog];

  // Switch Sync account to |identity2| should ask whether date should be
  // imported or kept separate. Choose to import the data.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SettingsImportDataImportButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SettingsImportDataContinueButton()]
      performAction:grey_tap()];

  // Check the signed-in user did change.
  [SigninEarlGreyUtils checkSignedInWithIdentity:identity2];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Opens the add account screen and then cancels it by opening a new tab.
// Ensures that the add account screen is correctly dismissed. crbug.com/462200
//
// TODO(crbug.com/962847): This test crashes when the the add account screen
// is dismissed.
- (void)DISABLED_testSignInCancelAddAccount {
  // Add an identity to avoid arriving on the Add Account screen when opening
  // sign-in.
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];

  // Open Add Account screen.
  id<GREYMatcher> add_account_matcher =
      chrome_test_util::StaticTextWithAccessibilityLabelId(
          unified_consent::IsUnifiedConsentFeatureEnabled()
              ? IDS_IOS_ACCOUNT_IDENTITY_CHOOSER_ADD_ACCOUNT
              : IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_ADD_ACCOUNT_BUTTON);
  [[EarlGrey selectElementWithMatcher:add_account_matcher]
      performAction:grey_tap()];
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  // Open new tab to cancel sign-in.
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:GURL("about:blank")];
  [chrome_test_util::DispatcherForActiveBrowserViewController()
      openURLInNewTab:command];

  // Re-open the sign-in screen. If it wasn't correctly dismissed previously,
  // this will fail.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];
  if (unified_consent::IsUnifiedConsentFeatureEnabled())
    [SigninEarlGreyUI selectIdentityWithEmail:identity.userEmail];
  VerifyChromeSigninViewVisible();

  // Close sign-in screen and Settings.
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON);
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

@end
