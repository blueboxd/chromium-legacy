// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <EarlGrey/GREYKeyboard.h>

#include "base/ios/ios_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/ios/browser/autofill_switches.h"
#include "components/keyed_service/core/service_access_type.h"
#import "ios/chrome/browser/autofill/form_suggestion_constants.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/address_mediator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/address_view_controller.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_accessory_view_controller.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_profile_table_view_controller.h"
#import "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#include "ios/web/public/test/element_selector.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::CancelButton;
using chrome_test_util::GetOriginalBrowserState;
using ios::ChromeBrowserState;
using manual_fill::AccessoryAddressAccessibilityIdentifier;
using manual_fill::AccessoryKeyboardAccessibilityIdentifier;
using manual_fill::AddressTableViewAccessibilityIdentifier;
using manual_fill::ManageAddressAccessibilityIdentifier;

namespace {

constexpr char kFormElementName[] = "name";
constexpr char kFormElementCity[] = "city";

constexpr char kFormHTMLFile[] = "/profile_form.html";

// Returns a matcher for the scroll view in keyboard accessory bar.
id<GREYMatcher> FormSuggestionViewMatcher() {
  return grey_accessibilityID(kFormSuggestionsViewAccessibilityIdentifier);
}

// Returns a matcher for the profiles icon in the keyboard accessory bar.
id<GREYMatcher> ProfilesIconMatcher() {
  return grey_accessibilityID(AccessoryAddressAccessibilityIdentifier);
}

// Matcher for the Keyboard icon in the accessory bar.
id<GREYMatcher> KeyboardIconMatcher() {
  return grey_accessibilityID(AccessoryKeyboardAccessibilityIdentifier);
}

// Returns a matcher for the profiles table view in manual fallback.
id<GREYMatcher> ProfilesTableViewMatcher() {
  return grey_accessibilityID(AddressTableViewAccessibilityIdentifier);
}

// Returns a matcher for the button to open profile settings in manual
// fallback.
id<GREYMatcher> ManageProfilesMatcher() {
  return grey_accessibilityID(ManageAddressAccessibilityIdentifier);
}

// Returns the matcher for an enabled cancel button in a navigation bar.
id<GREYMatcher> NavigationBarCancelMatcher() {
  return grey_allOf(
      grey_ancestor(grey_kindOfClass([UINavigationBar class])), CancelButton(),
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitNotEnabled)), nil);
}

// Returns a matcher for the profiles settings collection view.
id<GREYMatcher> ProfileSettingsMatcher() {
  return grey_accessibilityID(kAutofillProfileTableViewID);
}

// Returns a matcher for the ProfileTableView window.
id<GREYMatcher> ProfileTableViewWindowMatcher() {
  id<GREYMatcher> classMatcher = grey_kindOfClass([UIWindow class]);
  id<GREYMatcher> parentMatcher = grey_descendant(ProfilesTableViewMatcher());
  return grey_allOf(classMatcher, parentMatcher, nil);
}

// Saves an example profile in the store.
void AddAutofillProfile(autofill::PersonalDataManager* personalDataManager) {
  using base::test::ios::WaitUntilConditionOrTimeout;
  using base::test::ios::kWaitForActionTimeout;

  autofill::AutofillProfile profile = autofill::test::GetFullProfile();
  size_t previousProfileCount = personalDataManager->GetProfiles().size();
  personalDataManager->AddProfile(profile);
  GREYAssert(WaitUntilConditionOrTimeout(
                 kWaitForActionTimeout,
                 ^bool() {
                   return previousProfileCount <
                          personalDataManager->GetProfiles().size();
                 }),
             @"Failed to add profile.");
}

void ClearProfiles(autofill::PersonalDataManager* personalDataManager) {
  for (const auto* profile : personalDataManager->GetProfiles()) {
    personalDataManager->RemoveByGUID(profile->guid());
  }
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForActionTimeout,
                 ^bool() {
                   return 0 == personalDataManager->GetProfiles().size();
                 }),
             @"Failed to clean profiles.");
}

}  // namespace

// Integration Tests for Mannual Fallback Addresses View Controller.
@interface AddressViewControllerTestCase : ChromeTestCase {
  // The PersonalDataManager instance for the current browser state.
  autofill::PersonalDataManager* _personalDataManager;
}

@end

@implementation AddressViewControllerTestCase

+ (void)setUp {
  [super setUp];
  // If the previous run was manually stopped then the profile will be in the
  // store and the test will fail. We clean it here for those cases.
  ios::ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();
  autofill::PersonalDataManager* personalDataManager =
      autofill::PersonalDataManagerFactory::GetForBrowserState(browserState);
  ClearProfiles(personalDataManager);
}

- (void)setUp {
  [super setUp];
  ChromeBrowserState* browserState = GetOriginalBrowserState();
  _personalDataManager =
      autofill::PersonalDataManagerFactory::GetForBrowserState(browserState);
  _personalDataManager->SetSyncingForTest(true);
  // Store one address.
  AddAutofillProfile(_personalDataManager);

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL URL = self.testServer->GetURL(kFormHTMLFile);
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"Profile form"];
}

- (void)tearDown {
  ClearProfiles(_personalDataManager);
  [super tearDown];
}

// Tests that the addresses view controller appears on screen.
- (void)testAddressesViewControllerIsPresented {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementName)];

  // Tap on the addresses icon.
  [[EarlGrey selectElementWithMatcher:FormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
  [[EarlGrey selectElementWithMatcher:ProfilesIconMatcher()]
      performAction:grey_tap()];

  // Verify the address controller table view is visible.
  [[EarlGrey selectElementWithMatcher:ProfilesTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the addresses view controller contains the "Manage Addresses..."
// action.
- (void)testAddressesViewControllerContainsManageAddressesAction {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementName)];

  // Tap on the addresses icon.
  [[EarlGrey selectElementWithMatcher:FormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
  [[EarlGrey selectElementWithMatcher:ProfilesIconMatcher()]
      performAction:grey_tap()];

  // Verify the address controller contains the "Manage Addresses..." action.
  [[EarlGrey selectElementWithMatcher:ProfilesTableViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
  [[EarlGrey selectElementWithMatcher:ManageProfilesMatcher()]
      assertWithMatcher:grey_interactable()];
}

// Tests that the "Manage Addresses..." action works.
- (void)testManageAddressesActionOpensAddressSettings {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementName)];

  // Tap on the addresses icon.
  [[EarlGrey selectElementWithMatcher:FormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
  [[EarlGrey selectElementWithMatcher:ProfilesIconMatcher()]
      performAction:grey_tap()];

  // Tap the "Manage Addresses..." action.
  [[EarlGrey selectElementWithMatcher:ProfilesTableViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
  [[EarlGrey selectElementWithMatcher:ManageProfilesMatcher()]
      performAction:grey_tap()];

  // Verify the address settings opened.
  [[EarlGrey selectElementWithMatcher:ProfileSettingsMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that returning from "Manage Addresses..." leaves the icons and keyboard
// in the right state.
- (void)testAddressesStateAfterPresentingManageAddresses {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementName)];

  // Tap on the addresses icon.
  [[EarlGrey selectElementWithMatcher:FormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
  [[EarlGrey selectElementWithMatcher:ProfilesIconMatcher()]
      performAction:grey_tap()];

  // Verify the status of the icon.
  [[EarlGrey selectElementWithMatcher:ProfilesIconMatcher()]
      assertWithMatcher:grey_not(grey_userInteractionEnabled())];

  // Tap the "Manage Addresses..." action.
  [[EarlGrey selectElementWithMatcher:ProfilesTableViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
  [[EarlGrey selectElementWithMatcher:ManageProfilesMatcher()]
      performAction:grey_tap()];

  // Verify the address settings opened.
  [[EarlGrey selectElementWithMatcher:ProfileSettingsMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap Cancel Button.
  [[EarlGrey selectElementWithMatcher:NavigationBarCancelMatcher()]
      performAction:grey_tap()];

  // Verify the address settings closed.
  [[EarlGrey selectElementWithMatcher:ProfileSettingsMatcher()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  // Verify the status of the icons.
  [[EarlGrey selectElementWithMatcher:ProfilesIconMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:ProfilesIconMatcher()]
      assertWithMatcher:grey_userInteractionEnabled()];
  [[EarlGrey selectElementWithMatcher:KeyboardIconMatcher()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  // Verify the keyboard is not cover by the profiles view.
  [[EarlGrey selectElementWithMatcher:ProfilesTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the Address View Controller is dismissed when tapping the
// keyboard icon.
- (void)testKeyboardIconDismissAddressController {
  if ([ChromeEarlGrey isIPadIdiom]) {
    // The keyboard icon is never present in iPads.
    EARL_GREY_TEST_SKIPPED(@"Test is not applicable for iPad");
    ;
  }
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementName)];

  // Tap on the addresses icon.
  [[EarlGrey selectElementWithMatcher:FormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
  [[EarlGrey selectElementWithMatcher:ProfilesIconMatcher()]
      performAction:grey_tap()];

  // Verify the address controller table view is visible.
  [[EarlGrey selectElementWithMatcher:ProfilesTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on the keyboard icon.
  [[EarlGrey selectElementWithMatcher:KeyboardIconMatcher()]
      performAction:grey_tap()];

  // Verify the address controller table view and the address icon is NOT
  // visible.
  [[EarlGrey selectElementWithMatcher:ProfilesTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:KeyboardIconMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the Address View Controller is dismissed when tapping the outside
// the popover on iPad.
- (void)testIPadTappingOutsidePopOverDismissAddressController {
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test is not applicable for iPhone");
  }
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementName)];

  // Tap on the addresses icon.
  [[EarlGrey selectElementWithMatcher:FormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
  [[EarlGrey selectElementWithMatcher:ProfilesIconMatcher()]
      performAction:grey_tap()];

  // Verify the address controller table view is visible.
  [[EarlGrey selectElementWithMatcher:ProfilesTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on a point outside of the popover.
  // The way EarlGrey taps doesn't go through the window hierarchy. Because of
  // this, the tap needs to be done in the same window as the popover.
  [[EarlGrey selectElementWithMatcher:ProfileTableViewWindowMatcher()]
      performAction:grey_tapAtPoint(CGPointMake(0, 0))];

  // Verify the address controller table view and the address icon is NOT
  // visible.
  [[EarlGrey selectElementWithMatcher:ProfilesTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:KeyboardIconMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the address icon is hidden when no addresses are available.
- (void)testAddressIconIsNotVisibleWhenAddressStoreEmpty {
  // Delete the profile that is added on |-setUp|.
  ClearProfiles(_personalDataManager);

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementName)];

  // Wait for the keyboard to appear.
  [GREYKeyboard waitForKeyboardToAppear];

  // Assert the address icon is not visible.
  [[EarlGrey selectElementWithMatcher:ProfilesIconMatcher()]
      assertWithMatcher:grey_notVisible()];

  // Store one address.
  AddAutofillProfile(_personalDataManager);

  // Tap another field to trigger form activity.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementCity)];

  // Assert the address icon is visible now.
  [[EarlGrey selectElementWithMatcher:FormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
  // Verify the status of the icons.
  [[EarlGrey selectElementWithMatcher:ProfilesIconMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:ProfilesIconMatcher()]
      assertWithMatcher:grey_userInteractionEnabled()];
  [[EarlGrey selectElementWithMatcher:KeyboardIconMatcher()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
}

@end
