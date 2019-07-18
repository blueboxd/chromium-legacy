// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#import "base/ios/block_types.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/unified_consent/feature.h"
#import "ios/chrome/app/main_controller.h"
#include "ios/chrome/browser/system_flags.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view.h"
#import "ios/chrome/browser/ui/authentication/chrome_signin_view_controller.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui.h"
#import "ios/chrome/browser/ui/authentication/signin_earlgrey_utils.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_constants.h"
#import "ios/chrome/browser/ui/signin_interaction/signin_interaction_controller_egtest_util.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#import "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::BookmarksNavigationBarDoneButton;
using chrome_test_util::PrimarySignInButton;
using chrome_test_util::SecondarySignInButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SyncSettingsConfirmButton;

typedef NS_ENUM(NSInteger, OpenSigninMethod) {
  OpenSigninMethodFromSettings,
  OpenSigninMethodFromBookmarks,
  OpenSigninMethodFromRecentTabs,
  OpenSigninMethodFromTabSwitcher,
};

namespace {

// Taps on the primary sign-in button in recent tabs, and scroll first, if
// necessary.
void TapOnPrimarySignInButtonInRecentTabs() {
  id<GREYMatcher> matcher =
      grey_allOf(PrimarySignInButton(), grey_sufficientlyVisible(), nil);
  const CGFloat kPixelsToScroll = 300;
  id<GREYAction> searchAction =
      grey_scrollInDirection(kGREYDirectionDown, kPixelsToScroll);
  GREYElementInteraction* interaction =
      [[EarlGrey selectElementWithMatcher:matcher]
             usingSearchAction:searchAction
          onElementWithMatcher:
              grey_accessibilityID(
                  kRecentTabsTableViewControllerAccessibilityIdentifier)];
  [interaction performAction:grey_tap()];
}

// Removes all browsing data.
void RemoveBrowsingData() {
  __block BOOL browsing_data_removed = NO;
  [chrome_test_util::GetMainController()
      removeBrowsingDataForBrowserState:chrome_test_util::
                                            GetOriginalBrowserState()
                             timePeriod:browsing_data::TimePeriod::ALL_TIME
                             removeMask:BrowsingDataRemoveMask::REMOVE_ALL
                        completionBlock:^{
                          browsing_data_removed = YES;
                        }];
  GREYCondition* condition =
      [GREYCondition conditionWithName:@"Wait for removing browsing data."
                                 block:^BOOL {
                                   return browsing_data_removed;
                                 }];
  GREYAssert([condition waitWithTimeout:base::test::ios::kWaitForActionTimeout],
             @"Browsing data was not removed.");
}

}  // namespace

// Sign-in interaction tests that work both with Unified Consent enabled or
// disabled.
@interface SigninInteractionControllerTestCase : ChromeTestCase
@end

@implementation SigninInteractionControllerTestCase

- (void)setUp {
  [super setUp];
  // Remove closed tab history to make sure the sign-in promo is always visible
  // in recent tabs.
  RemoveBrowsingData();
}

// Tests that opening the sign-in screen from the Settings and signing in works
// correctly when there is already an identity on the device.
- (void)testSignInOneUser {
  // Set up a fake identity.
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  [SigninEarlGreyUI signinWithIdentity:identity];

  // Check |identity| is signed-in.
  [SigninEarlGreyUtils checkSignedInWithIdentity:identity];
}

// Tests that signing out from the Settings works correctly.
- (void)testSignInDisconnectFromChrome {
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  // Sign in to |identity|.
  [SigninEarlGreyUI signinWithIdentity:identity];
  [ChromeEarlGreyUI openSettingsMenu];

  // Go to Accounts Settings and tap the sign out button.
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
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];

  // Check that there is no signed in user.
  [SigninEarlGreyUtils checkSignedOut];
}

// Tests that signing out of a managed account from the Settings works
// correctly.
// TODO(crbug.com/929967): Re-enable the test.
- (void)DISABLED_testSignInDisconnectFromChromeManaged {
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeManagedIdentity];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];
  [SigninEarlGreyUI selectIdentityWithEmail:identity.userEmail];

  // Synchronization off due to an infinite spinner.
  SetEarlGreySynchronizationEnabled(NO);
  WaitForMatcher(chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_IOS_MANAGED_SIGNIN_ACCEPT_BUTTON));
  TapButtonWithLabelId(IDS_IOS_MANAGED_SIGNIN_ACCEPT_BUTTON);
  SetEarlGreySynchronizationEnabled(YES);

  [SigninEarlGreyUI confirmSigninConfirmationDialog];
  [SigninEarlGreyUtils checkSignedInWithIdentity:identity];

  // Go to Accounts Settings and tap the sign out button.
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
  TapButtonWithLabelId(IDS_IOS_MANAGED_DISCONNECT_DIALOG_ACCEPT);
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];

  // Check that there is no signed in user.
  [SigninEarlGreyUtils checkSignedOut];
}

// Tests that signing in, tapping the Settings link on the confirmation screen
// and closing the Settings correctly leaves the user signed in without any
// Settings shown.
- (void)testSignInOpenSettings {
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];
  [SigninEarlGreyUI selectIdentityWithEmail:identity.userEmail];

  // Wait until the next screen appears.
  [SigninEarlGreyUI tapSettingsLink];

  if (unified_consent::IsUnifiedConsentFeatureEnabled()) {
    [[EarlGrey selectElementWithMatcher:SyncSettingsConfirmButton()]
        performAction:grey_tap()];
  } else {
    [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
        performAction:grey_tap()];
  }

  // All Settings should be gone and user signed in.
  id<GREYMatcher> settings_matcher =
      chrome_test_util::StaticTextWithAccessibilityLabelId(
          IDS_IOS_SETTINGS_TITLE);
  [[EarlGrey selectElementWithMatcher:settings_matcher]
      assertWithMatcher:grey_notVisible()];
  [SigninEarlGreyUtils checkSignedInWithIdentity:identity];
}

// Opens the sign in screen and then cancel it by opening a new tab. Ensures
// that the sign in screen is correctly dismissed. crbug.com/462200
- (void)testSignInCancelIdentityPicker {
  // Add an identity to avoid arriving on the Add Account screen when opening
  // sign-in.
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];

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

// Starts an authentication flow and cancel it by opening a new tab. Ensures
// that the authentication flow is correctly canceled and dismissed.
// crbug.com/462202
- (void)testSignInCancelAuthenticationFlow {
  // Set up the fake identities.
  ios::FakeChromeIdentityService* identity_service =
      ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
  ChromeIdentity* identity1 = [SigninEarlGreyUtils fakeIdentity1];
  ChromeIdentity* identity2 = [SigninEarlGreyUtils fakeIdentity2];
  identity_service->AddIdentity(identity1);
  identity_service->AddIdentity(identity2);

  // This signs in |identity2| first, ensuring that the "Clear Data Before
  // Syncing" dialog is shown during the second sign-in. This dialog will
  // effectively block the authentication flow, ensuring that the authentication
  // flow is always still running when the sign-in is being cancelled.
  [SigninEarlGreyUI signinWithIdentity:identity2];
  [ChromeEarlGreyUI openSettingsMenu];

  // Go to Accounts Settings and tap the sign out button.
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

  // Sign in with |identity1|.
  [[EarlGrey selectElementWithMatcher:SecondarySignInButton()]
      performAction:grey_tap()];
  [SigninEarlGreyUI selectIdentityWithEmail:identity1.userEmail];
  if (unified_consent::IsUnifiedConsentFeatureEnabled()) {
    // With unified consent, the authentication flow is only created when the
    // confirm button is selected. Note that authentication flow actually
    /// blocks as the "Clear Browsing Before Syncing" dialog is presented.
    [SigninEarlGreyUI confirmSigninConfirmationDialog];
  }

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
    [SigninEarlGreyUI selectIdentityWithEmail:identity1.userEmail];
  VerifyChromeSigninViewVisible();

  // Close sign-in screen and Settings.
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON);
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
  [SigninEarlGreyUtils checkSignedOut];
}

// Opens the sign in screen from the bookmarks and then cancel it by tapping on
// done. Ensures that the sign in screen is correctly dismissed.
// Regression test for crbug.com/596029.
- (void)testSignInCancelFromBookmarks {
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  // Open Bookmarks and tap on Sign In promo button.
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI tapToolsMenuButton:chrome_test_util::BookmarksMenuButton()];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];

  // Assert sign-in screen was shown.
  if (unified_consent::IsUnifiedConsentFeatureEnabled())
    [SigninEarlGreyUI selectIdentityWithEmail:identity.userEmail];
  VerifyChromeSigninViewVisible();

  // Open new tab to cancel sign-in.
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:GURL("about:blank")];
  [chrome_test_util::DispatcherForActiveBrowserViewController()
      openURLInNewTab:command];

  // Re-open the sign-in screen. If it wasn't correctly dismissed previously,
  // this will fail.
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI tapToolsMenuButton:chrome_test_util::BookmarksMenuButton()];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];
  if (unified_consent::IsUnifiedConsentFeatureEnabled())
    [SigninEarlGreyUI selectIdentityWithEmail:identity.userEmail];
  VerifyChromeSigninViewVisible();

  // Close sign-in screen and Bookmarks.
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON);
  [[EarlGrey selectElementWithMatcher:BookmarksNavigationBarDoneButton()]
      performAction:grey_tap()];
}

#pragma mark - Dismiss tests

// Tests to dismiss sign-in by opening an URL from another app.
// Sign-in opened from: setting menu.
// Interrupted at: user consent.
- (void)testDismissSigninFromSettings {
  [self assertOpenURLWhenSigninFromView:OpenSigninMethodFromSettings
                        tapSettingsLink:NO];
}

// Tests to dismiss sign-in by opening an URL from another app.
// Sign-in opened from: setting menu.
// Interrupted at: advanced sign-in.
- (void)testDismissAdvancedSigninSettingsFromAdvancedSigninSettings {
  [self assertOpenURLWhenSigninFromView:OpenSigninMethodFromSettings
                        tapSettingsLink:YES];
}

// Tests to dismiss sign-in by opening an URL from another app.
// Sign-in opened from: bookmark view.
// Interrupted at: user consent.
- (void)testDismissSigninFromBookmarks {
  [self assertOpenURLWhenSigninFromView:OpenSigninMethodFromBookmarks
                        tapSettingsLink:NO];
}

// Tests to dismiss sign-in by opening an URL from another app.
// Sign-in opened from: bookmark view.
// Interrupted at: advanced sign-in.
- (void)testDismissAdvancedSigninBookmarksFromAdvancedSigninSettings {
  [self assertOpenURLWhenSigninFromView:OpenSigninMethodFromBookmarks
                        tapSettingsLink:YES];
}

// Tests to dismiss sign-in by opening an URL from another app.
// Sign-in opened from: recent tabs.
// Interrupted at: user consent.
- (void)testDismissSigninFromRecentTabs {
  [self assertOpenURLWhenSigninFromView:OpenSigninMethodFromRecentTabs
                        tapSettingsLink:NO];
}

// Tests to dismiss sign-in by opening an URL from another app.
// Sign-in opened from: recent tabs.
// Interrupted at: advanced sign-in.
- (void)testDismissSigninFromRecentTabsFromAdvancedSigninSettings {
  [self assertOpenURLWhenSigninFromView:OpenSigninMethodFromRecentTabs
                        tapSettingsLink:YES];
}

// Tests to dismiss sign-in by opening an URL from another app.
// Sign-in opened from: tab switcher.
// Interrupted at: user consent.
- (void)testDismissSigninFromTabSwitcher {
  [self assertOpenURLWhenSigninFromView:OpenSigninMethodFromTabSwitcher
                        tapSettingsLink:NO];
}

// Tests to dismiss sign-in by opening an URL from another app.
// Sign-in opened from: tab switcher.
// Interrupted at: advanced sign-in.
- (void)testDismissSigninFromTabSwitcherFromAdvancedSigninSettings {
  [self assertOpenURLWhenSigninFromView:OpenSigninMethodFromTabSwitcher
                        tapSettingsLink:YES];
}

#pragma mark - Utils

// Starts the sign-in workflow, and simulates opening an URL from another app.
// |openSigninMethod| is the way to start the sign-in.
// |tapSettingsLink| if YES, the setting link is tapped before opening the URL.
- (void)assertOpenURLWhenSigninFromView:(OpenSigninMethod)openSigninMethod
                        tapSettingsLink:(BOOL)tapSettingsLink {
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);
  switch (openSigninMethod) {
    case OpenSigninMethodFromSettings:
      [ChromeEarlGreyUI openSettingsMenu];
      [ChromeEarlGreyUI tapSettingsMenuButton:PrimarySignInButton()];
      break;
    case OpenSigninMethodFromBookmarks:
      [ChromeEarlGreyUI openToolsMenu];
      [ChromeEarlGreyUI
          tapToolsMenuButton:chrome_test_util::BookmarksMenuButton()];
      [[EarlGrey selectElementWithMatcher:PrimarySignInButton()]
          performAction:grey_tap()];
      break;
    case OpenSigninMethodFromRecentTabs:
      [ChromeEarlGreyUI openToolsMenu];
      [ChromeEarlGreyUI
          tapToolsMenuButton:chrome_test_util::RecentTabsMenuButton()];
      TapOnPrimarySignInButtonInRecentTabs();
      break;
    case OpenSigninMethodFromTabSwitcher:
      [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridOpenButton()]
          performAction:grey_tap()];
      [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                              TabGridOtherDevicesPanelButton()]
          performAction:grey_tap()];
      TapOnPrimarySignInButtonInRecentTabs();
      break;
  }
  if (tapSettingsLink) {
    [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
    [SigninEarlGreyUI tapSettingsLink];
  }
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  // Open the URL as if it was opened from another app.
  UIApplication* application = UIApplication.sharedApplication;
  id<UIApplicationDelegate> applicationDelegate = application.delegate;
  NSURL* url = [NSURL URLWithString:@"http://www.example.com/"];
  [applicationDelegate application:application openURL:url options:@{}];
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  // Check if the URL was opened.
  web::WebState* webState = chrome_test_util::GetMainController()
                                .interfaceProvider.mainInterface.tabModel
                                .webStateList->GetActiveWebState();
  GURL expectedString(url.absoluteString.UTF8String);
  GREYAssertEqual(expectedString, webState->GetVisibleURL(), @"url not loaded");
  if (tapSettingsLink) {
    // Should be signed in.
    [SigninEarlGreyUtils checkSignedInWithIdentity:identity];
  } else {
    // Should be not signed in.
    [SigninEarlGreyUtils checkSignedOut];
  }
}

@end
