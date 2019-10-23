// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <EarlGrey/GREYAppleInternals.h>
#import <EarlGrey/GREYKeyboard.h>
#include <atomic>

#include "base/ios/ios_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "ios/chrome/browser/application_context.h"
#import "ios/chrome/browser/autofill/form_suggestion_constants.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/address_view_controller.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_accessory_view_controller.h"
#import "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#include "ios/web/public/test/element_selector.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using manual_fill::AccessoryKeyboardAccessibilityIdentifier;

namespace {

constexpr char kFormElementName[] = "name";
constexpr char kFormElementCity[] = "city";

constexpr char kFormHTMLFile[] = "/profile_form.html";

// EarlGrey fails to detect undocked keyboards on screen, so this help check
// for them.
static std::atomic_bool gCHRIsKeyboardShown(false);

// Returns a matcher for the scroll view in keyboard accessory bar.
id<GREYMatcher> FormSuggestionViewMatcher() {
  return grey_accessibilityID(kFormSuggestionsViewAccessibilityIdentifier);
}

// Returns a matcher for the profiles icon in the keyboard accessory bar.
id<GREYMatcher> ProfilesIconMatcher() {
  return grey_accessibilityID(
      manual_fill::AccessoryAddressAccessibilityIdentifier);
}

// Returns a matcher for the profiles table view in manual fallback.
id<GREYMatcher> ProfilesTableViewMatcher() {
  return grey_accessibilityID(
      manual_fill::AddressTableViewAccessibilityIdentifier);
}

// Returns a matcher for the profiles table view in manual fallback.
id<GREYMatcher> SuggestionViewMatcher() {
  return grey_accessibilityID(kFormSuggestionLabelAccessibilityIdentifier);
}

// Returns a matcher for the ProfileTableView's window.
id<GREYMatcher> ProfilesTableViewWindowMatcher() {
  id<GREYMatcher> classMatcher = grey_kindOfClass([UIWindow class]);
  id<GREYMatcher> parentMatcher = grey_descendant(ProfilesTableViewMatcher());
  return grey_allOf(classMatcher, parentMatcher, nil);
}

// Returns a matcher for a button in the ProfileTableView. Currently it returns
// the company one.
id<GREYMatcher> ProfileTableViewButtonMatcher() {
  // The company name for autofill::test::GetFullProfile() is "Underworld".
  return grey_buttonTitle(@"Underworld");
}

// Matcher for the Keyboard icon in the accessory bar.
id<GREYMatcher> KeyboardIconMatcher() {
  return grey_accessibilityID(AccessoryKeyboardAccessibilityIdentifier);
}

// Saves an example profile in the store.
void AddAutofillProfile(autofill::PersonalDataManager* personalDataManager) {
  autofill::AutofillProfile profile = autofill::test::GetFullProfile();
  size_t profileCount = personalDataManager->GetProfiles().size();
  personalDataManager->AddProfile(profile);
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForActionTimeout,
                 ^bool() {
                   return profileCount <
                          personalDataManager->GetProfiles().size();
                 }),
             @"Failed to add profile.");
}

// Polls the JavaScript query |java_script_condition| until the returned
// |boolValue| is YES with a kWaitForActionTimeout timeout.
BOOL WaitForJavaScriptCondition(NSString* java_script_condition) {
  auto verify_block = ^BOOL {
    id boolValue =
        chrome_test_util::ExecuteJavaScript(java_script_condition, nil);
    return [boolValue isEqual:@YES];
  };
  NSTimeInterval timeout = base::test::ios::kWaitForActionTimeout;
  NSString* condition_name = [NSString
      stringWithFormat:@"Wait for JS condition: %@", java_script_condition];
  GREYCondition* condition = [GREYCondition conditionWithName:condition_name
                                                        block:verify_block];
  return [condition waitWithTimeout:timeout];
}

// If the keyboard is not present this will add a text field to the hierarchy,
// make it first responder and return it. If it is already present, this does
// nothing and returns nil.
UITextField* ShowKeyboard() {
  UITextField* textField = nil;
  if (!gCHRIsKeyboardShown) {
    CGRect rect = CGRectMake(0, 0, 300, 100);
    textField = [[UITextField alloc] initWithFrame:rect];
    textField.backgroundColor = [UIColor blueColor];
    [[[UIApplication sharedApplication] keyWindow] addSubview:textField];
    [textField becomeFirstResponder];
  }
  auto verify_block = ^BOOL {
    return gCHRIsKeyboardShown;
  };
  NSTimeInterval timeout = base::test::ios::kWaitForUIElementTimeout;
  NSString* condition_name =
      [NSString stringWithFormat:@"Wait for keyboard to appear"];
  GREYCondition* condition = [GREYCondition conditionWithName:condition_name
                                                        block:verify_block];
  [condition waitWithTimeout:timeout];
  return textField;
}

// Returns the dismiss key if present in the passed keyboard layout. Returns nil
// if not found.
UIAccessibilityElement* KeyboardDismissKeyInLayout(UIView* layout) {
  UIAccessibilityElement* key = nil;
  if ([layout accessibilityElementCount] != NSNotFound) {
    for (NSInteger i = [layout accessibilityElementCount]; i >= 0; --i) {
      id element = [layout accessibilityElementAtIndex:i];
      if ([[[element key] valueForKey:@"name"]
              isEqualToString:@"Dismiss-Key"]) {
        key = element;
        break;
      }
    }
  }
  return key;
}

// Matcher for the Keyboard Window.
id<GREYMatcher> KeyboardWindow(UIView* layout) {
  id<GREYMatcher> classMatcher = grey_kindOfClass([UIWindow class]);
  UIAccessibilityElement* key = KeyboardDismissKeyInLayout(layout);
  id<GREYMatcher> parentMatcher =
      grey_descendant(grey_accessibilityLabel(key.accessibilityLabel));
  return grey_allOf(classMatcher, parentMatcher, nil);
}

// Returns YES if the keyboard is docked at the bottom. NO otherwise.
BOOL IsKeyboardDockedForLayout(UIView* layout) {
  CGRect windowBounds = layout.window.bounds;
  UIView* viewToCompare = layout;
  while (viewToCompare &&
         viewToCompare.bounds.size.height < windowBounds.size.height) {
    CGRect keyboardFrameInWindow =
        [viewToCompare.window convertRect:viewToCompare.bounds
                                 fromView:viewToCompare];

    CGFloat maxY = CGRectGetMaxY(keyboardFrameInWindow);
    if ([@(maxY) isEqualToNumber:@(windowBounds.size.height)]) {
      return YES;
    }
    viewToCompare = viewToCompare.superview;
  }
  return NO;
}

// Undocks and split the keyboard by swiping it up. Does nothing if already
// undocked. Some devices, like iPhone or iPad Pro, do not allow undocking or
// splitting, this returns NO if it is the case.
BOOL UndockAndSplitKeyboard() {
  if (![ChromeEarlGrey isIPadIdiom]) {
    return NO;
  }

  UITextField* textField = ShowKeyboard();

  // Assert the "Dismiss-Key" is present.
  UIView* layout = [[UIKeyboardImpl sharedInstance] _layout];
  GREYAssert([[layout valueForKey:@"keyplaneContainsDismissKey"] boolValue],
             @"No dismiss key is pressent");

  // Return if already undocked.
  if (!IsKeyboardDockedForLayout(layout)) {
    // If we created a dummy textfield for this, remove it.
    [textField removeFromSuperview];
    return YES;
  }

  // Swipe it up.
  if (!layout.accessibilityIdentifier.length) {
    layout.accessibilityIdentifier = @"CRKBLayout";
  }

  UIAccessibilityElement* key = KeyboardDismissKeyInLayout(layout);
  CGRect keyFrameInScreen = [key accessibilityFrame];
  CGRect keyFrameInWindow = [UIScreen.mainScreen.coordinateSpace
            convertRect:keyFrameInScreen
      toCoordinateSpace:layout.window.coordinateSpace];
  CGRect windowBounds = layout.window.bounds;

  CGPoint startPoint = CGPointMake(
      (keyFrameInWindow.origin.x + keyFrameInWindow.size.width / 2.0) /
          windowBounds.size.width,
      (keyFrameInWindow.origin.y + keyFrameInWindow.size.height / 2.0) /
          windowBounds.size.height);

  id action = grey_swipeFastInDirectionWithStartPoint(
      kGREYDirectionUp, startPoint.x, startPoint.y);

  [[EarlGrey selectElementWithMatcher:KeyboardWindow(layout)]
      performAction:action];

  return !IsKeyboardDockedForLayout(layout);
}

// Docks the keyboard by swiping it down. Does nothing if already docked.
void DockKeyboard() {
  if (![ChromeEarlGrey isIPadIdiom]) {
    return;
  }

  UITextField* textField = ShowKeyboard();

  // Assert the "Dismiss-Key" is present.
  UIView* layout = [[UIKeyboardImpl sharedInstance] _layout];
  GREYAssert([[layout valueForKey:@"keyplaneContainsDismissKey"] boolValue],
             @"No dismiss key is pressent");

  // Return if already docked.
  if (IsKeyboardDockedForLayout(layout)) {
    // If we created a dummy textfield for this, remove it.
    [textField removeFromSuperview];
    return;
  }

  // Swipe it down.
  UIAccessibilityElement* key = KeyboardDismissKeyInLayout(layout);

  CGRect keyFrameInScreen = [key accessibilityFrame];
  CGRect keyFrameInWindow = [UIScreen.mainScreen.coordinateSpace
            convertRect:keyFrameInScreen
      toCoordinateSpace:layout.window.coordinateSpace];
  CGRect windowBounds = layout.window.bounds;

  GREYAssertFalse(CGRectEqualToRect(keyFrameInWindow, CGRectZero),
                  @"The dismiss key accessibility frame musn't be zero");
  CGPoint startPoint = CGPointMake(
      (keyFrameInWindow.origin.x + keyFrameInWindow.size.width / 2.0) /
          windowBounds.size.width,
      (keyFrameInWindow.origin.y + keyFrameInWindow.size.height / 2.0) /
          windowBounds.size.height);
  id<GREYAction> action = grey_swipeFastInDirectionWithStartPoint(
      kGREYDirectionDown, startPoint.x, startPoint.y);

  [[EarlGrey selectElementWithMatcher:KeyboardWindow(layout)]
      performAction:action];

  // If we created a dummy textfield for this, remove it.
  [textField removeFromSuperview];

  GREYCondition* waitForDockedKeyboard =
      [GREYCondition conditionWithName:@"Wait For Docked Keyboard Animations"
                                 block:^BOOL {
                                   return IsKeyboardDockedForLayout(layout);
                                 }];

  GREYAssertTrue([waitForDockedKeyboard
                     waitWithTimeout:base::test::ios::kWaitForActionTimeout],
                 @"Keyboard animations still present.");
}

}  // namespace

// Integration Tests for fallback coordinator.
@interface FallbackCoordinatorTestCase : ChromeTestCase {
  // The PersonalDataManager instance for the current browser state.
  autofill::PersonalDataManager* _personalDataManager;
}

@end

@implementation FallbackCoordinatorTestCase

+ (void)load {
  @autoreleasepool {
    // EarlGrey fails to detect undocked keyboards on screen, so this help check
    // for them.
    auto block = ^(NSNotification* note) {
      CGRect keyboardFrame =
          [note.userInfo[UIKeyboardFrameEndUserInfoKey] CGRectValue];
      UIWindow* window = [UIApplication sharedApplication].keyWindow;
      keyboardFrame = [window convertRect:keyboardFrame fromWindow:nil];
      CGRect windowFrame = window.frame;
      CGRect frameIntersection = CGRectIntersection(windowFrame, keyboardFrame);
      gCHRIsKeyboardShown =
          frameIntersection.size.width > 1 && frameIntersection.size.height > 1;
    };

    [[NSNotificationCenter defaultCenter]
        addObserverForName:UIKeyboardDidChangeFrameNotification
                    object:nil
                     queue:nil
                usingBlock:block];

    [[NSNotificationCenter defaultCenter]
        addObserverForName:UIKeyboardDidShowNotification
                    object:nil
                     queue:nil
                usingBlock:block];

    [[NSNotificationCenter defaultCenter]
        addObserverForName:UIKeyboardDidHideNotification
                    object:nil
                     queue:nil
                usingBlock:block];
  }
}

+ (void)setUp {
  [super setUp];
  // If the previous run was manually stopped then the profile will be in the
  // store and the test will fail. We clean it here for those cases.
  ios::ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();
  autofill::PersonalDataManager* personalDataManager =
      autofill::PersonalDataManagerFactory::GetForBrowserState(browserState);
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

- (void)setUp {
  [super setUp];
  ios::ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();
  _personalDataManager =
      autofill::PersonalDataManagerFactory::GetForBrowserState(browserState);
  _personalDataManager->SetSyncingForTest(true);
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL URL = self.testServer->GetURL(kFormHTMLFile);
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"Profile form"];
}

- (void)tearDown {
  for (const auto* profile : _personalDataManager->GetProfiles()) {
    _personalDataManager->RemoveByGUID(profile->guid());
  }
  // Leaving a picker on iPads causes problems with the docking logic. This
  // will dismiss any.
  if ([ChromeEarlGrey isIPadIdiom]) {
    // Tap in the web view so the popover dismisses.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
        performAction:grey_tapAtPoint(CGPointMake(0, 0))];

    // Verify the table view is not visible.
    [[EarlGrey selectElementWithMatcher:grey_kindOfClass([UITableView class])]
        assertWithMatcher:grey_notVisible()];
  }
  [super tearDown];
}

// Tests that the when tapping the outside the popover on iPad, suggestions
// continue working.
- (void)testIPadTappingOutsidePopOverResumesSuggestionsCorrectly {
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test not applicable for iPhone.");
  }

  GREYAssertEqual(_personalDataManager->GetProfiles().size(), 0,
                  @"Test started in an unclean state. Profiles were already "
                  @"present in the data manager.");

  // Add the profile to be tested.
  AddAutofillProfile(_personalDataManager);

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementName)];

  // Tap on the profiles icon.
  [[EarlGrey selectElementWithMatcher:ProfilesIconMatcher()]
      performAction:grey_tap()];

  // Verify the profiles controller table view is visible.
  [[EarlGrey selectElementWithMatcher:ProfilesTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on a point outside of the popover.
  // The way EarlGrey taps doesn't go through the window hierarchy. Because of
  // this, the tap needs to be done in the same window as the popover.
  [[EarlGrey selectElementWithMatcher:ProfilesTableViewWindowMatcher()]
      performAction:grey_tapAtPoint(CGPointMake(0, 0))];

  // Verify the profiles controller table view is NOT visible.
  [[EarlGrey selectElementWithMatcher:ProfilesTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];

  // Tap on the suggestion.
  [[EarlGrey selectElementWithMatcher:SuggestionViewMatcher()]
      performAction:grey_tap()];

  // Verify Web Content was filled.
  autofill::AutofillProfile* profile = _personalDataManager->GetProfiles()[0];
  base::string16 name =
      profile->GetInfo(autofill::AutofillType(autofill::NAME_FULL),
                       GetApplicationContext()->GetApplicationLocale());

  NSString* javaScriptCondition = [NSString
      stringWithFormat:@"document.getElementById('%s').value === '%@'",
                       kFormElementName, base::SysUTF16ToNSString(name)];
  XCTAssertTrue(WaitForJavaScriptCondition(javaScriptCondition));
}

// Tests that the manual fallback view concedes preference to the system picker
// for selection elements.
- (void)testPickerDismissesManualFallback {
  // Add the profile to be used.
  AddAutofillProfile(_personalDataManager);

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementCity)];

  // Tap on the profiles icon.
  [[EarlGrey selectElementWithMatcher:ProfilesIconMatcher()]
      performAction:grey_tap()];

  // Verify the profiles controller table view is visible.
  [[EarlGrey selectElementWithMatcher:ProfilesTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap any option.
  [[EarlGrey selectElementWithMatcher:ProfileTableViewButtonMatcher()]
      performAction:grey_tap()];

  // Verify the profiles controller table view is not visible.
  [[EarlGrey selectElementWithMatcher:ProfilesTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];

  // Verify the status of the icons.
  if ([ChromeEarlGrey isIPadIdiom]) {
    // Hidden on iPad.
    [[EarlGrey selectElementWithMatcher:ProfilesIconMatcher()]
        assertWithMatcher:grey_notVisible()];
    [[EarlGrey selectElementWithMatcher:KeyboardIconMatcher()]
        assertWithMatcher:grey_not(grey_sufficientlyVisible())];
  } else {
    [[EarlGrey selectElementWithMatcher:ProfilesIconMatcher()]
        assertWithMatcher:grey_sufficientlyVisible()];
    [[EarlGrey selectElementWithMatcher:ProfilesIconMatcher()]
        assertWithMatcher:grey_userInteractionEnabled()];
    [[EarlGrey selectElementWithMatcher:KeyboardIconMatcher()]
        assertWithMatcher:grey_not(grey_sufficientlyVisible())];
  }
}

// Tests that the input accessory view continues working after a picker is
// present.
- (void)testInputAccessoryBarIsPresentAfterPickers {
  // Add the profile to be used.
  AddAutofillProfile(_personalDataManager);

  // Bring up the keyboard by tapping the city, which is the element before the
  // picker.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementCity)];

  // Tap on the profiles icon.
  [[EarlGrey selectElementWithMatcher:FormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
  [[EarlGrey selectElementWithMatcher:ProfilesIconMatcher()]
      performAction:grey_tap()];

  // Verify the profiles controller table view is visible.
  [[EarlGrey selectElementWithMatcher:ProfilesTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap any option.
  [[EarlGrey selectElementWithMatcher:ProfileTableViewButtonMatcher()]
      performAction:grey_tap()];

  // Verify the profiles controller table view is not visible.
  [[EarlGrey selectElementWithMatcher:ProfilesTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];

  // On iPad the picker is a table view in a popover, we need to dismiss that
  // first.
  if ([ChromeEarlGrey isIPadIdiom]) {
    // Tap in the web view so the popover dismisses.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
        performAction:grey_tapAtPoint(CGPointMake(0, 0))];

    // Verify the table view is not visible.
    [[EarlGrey selectElementWithMatcher:grey_kindOfClass([UITableView class])]
        assertWithMatcher:grey_notVisible()];
  }

  // Bring up the regular keyboard again.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementCity)];

  // Wait for the accessory icon to appear.
  [GREYKeyboard waitForKeyboardToAppear];

  // Verify the profiles icon is visible, and therefore also the input accessory
  // bar.
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

// Same as before but with the keyboard undocked the re-docked.
- (void)testRedockedInputAccessoryBarIsPresentAfterPickers {
  // No need to run if not iPad.
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test not applicable for iPhone.");
  }

  // Add the profile to be used.
  AddAutofillProfile(_personalDataManager);

  // Bring up the keyboard by tapping the city, which is the element before the
  // picker.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementCity)];

  if (!UndockAndSplitKeyboard()) {
    EARL_GREY_TEST_DISABLED(
        @"Undocking the keyboard does not work on iPhone or iPad Pro");
  }

  // When keyboard is split, icons are not visible, so we rely on timeout before
  // docking again, because EarlGrey synchronization isn't working properly with
  // the keyboard.
  [self waitForMatcherToBeVisible:ProfilesIconMatcher()
                          timeout:base::test::ios::kWaitForUIElementTimeout];

  DockKeyboard();

  // Tap on the profiles icon.
  [[EarlGrey selectElementWithMatcher:ProfilesIconMatcher()]
      performAction:grey_tap()];

  // Verify the profiles controller table view is visible.
  [[EarlGrey selectElementWithMatcher:ProfilesTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap any option.
  [[EarlGrey selectElementWithMatcher:ProfileTableViewButtonMatcher()]
      performAction:grey_tap()];

  // Verify the profiles controller table view is not visible.
  [[EarlGrey selectElementWithMatcher:ProfilesTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];

  // On iPad the picker is a table view in a popover, we need to dismiss that
  // first. Tap in the previous field, so the popover dismisses.
  [[EarlGrey selectElementWithMatcher:grey_keyWindow()]
      performAction:grey_tapAtPoint(CGPointMake(0, 0))];

  // Verify the table view is not visible.
  [[EarlGrey selectElementWithMatcher:grey_kindOfClass([UITableView class])]
      assertWithMatcher:grey_notVisible()];

  // Bring up the regular keyboard again.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementName)];

  // Wait for the accessory icon to appear.
  [GREYKeyboard waitForKeyboardToAppear];

  // Verify the profiles icon is visible, and therefore also the input accessory
  // bar.
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

// Test the input accessory bar is present when undocking then docking the
// keyboard.
- (void)testInputAccessoryBarIsPresentAfterUndockingKeyboard {
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test not applicable for iPhone.");
  }

  // Add the profile to use for verification.
  AddAutofillProfile(_personalDataManager);

  // Bring up the keyboard by tapping the city, which is the element before the
  // picker.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementCity)];

  if (!UndockAndSplitKeyboard()) {
    EARL_GREY_TEST_DISABLED(
        @"Undocking the keyboard does not work on iPhone or iPad Pro");
  }

  // When keyboard is split, icons are not visible, so we rely on timeout before
  // docking again, because EarlGrey synchronization isn't working properly with
  // the keyboard.
  [self waitForMatcherToBeVisible:ProfilesIconMatcher()
                          timeout:base::test::ios::kWaitForUIElementTimeout];

  DockKeyboard();

  // Verify the profiles icon is visible, and therefore also the input accessory
  // bar.
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

// Tests that the manual fallback view is present in incognito.
- (void)testIncognitoManualFallbackMenu {
  // Add the profile to use for verification.
  AddAutofillProfile(_personalDataManager);

  // Bring up the keyboard by tapping the city, which is the element before the
  // picker.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementCity)];

  // Verify the profiles icon is visible.
  [[EarlGrey selectElementWithMatcher:FormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
  [[EarlGrey selectElementWithMatcher:ProfilesIconMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Open a tab in incognito.
  [ChromeEarlGrey openNewIncognitoTab];
  const GURL URL = self.testServer->GetURL(kFormHTMLFile);
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"Profile form"];

  // Bring up the keyboard by tapping the city, which is the element before the
  // picker.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementCity)];

  // Verify the profiles icon is visible.
  [[EarlGrey selectElementWithMatcher:FormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
  [[EarlGrey selectElementWithMatcher:ProfilesIconMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests the mediator stops observing objects when the incognito BVC is
// destroyed. Waiting for dealloc was causing a race condition with the
// autorelease pool, and some times a DCHECK will be hit.
- (void)testOpeningIncognitoTabsDoNotLeak {
  const GURL URL = self.testServer->GetURL(kFormHTMLFile);
  std::string webViewText("Profile form");
  AddAutofillProfile(_personalDataManager);

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementCity)];

  // Verify the profiles icon is visible.
  [[EarlGrey selectElementWithMatcher:FormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
  [[EarlGrey selectElementWithMatcher:ProfilesIconMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Open a tab in incognito.
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:webViewText];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementCity)];

  // Verify the profiles icon is visible.
  [[EarlGrey selectElementWithMatcher:FormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
  [[EarlGrey selectElementWithMatcher:ProfilesIconMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [ChromeEarlGrey closeCurrentTab];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:webViewText];

  // Bring up the keyboard by tapping the city, which is the element before the
  // picker.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementCity)];

  // Verify the profiles icon is visible.
  [[EarlGrey selectElementWithMatcher:FormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
  [[EarlGrey selectElementWithMatcher:ProfilesIconMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Open a tab in incognito.
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:webViewText];

  // Bring up the keyboard by tapping the city, which is the element before the
  // picker.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementCity)];

  // Open a  regular tab.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:webViewText];

  // Bring up the keyboard by tapping the city, which is the element before the
  // picker.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementCity)];

  // This will fail if there is more than one profiles icon in the hierarchy.
  [[EarlGrey selectElementWithMatcher:FormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
  [[EarlGrey selectElementWithMatcher:ProfilesIconMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the manual fallback view is not duplicated after incognito.
- (void)testReturningFromIncognitoDoesNotDuplicatesManualFallbackMenu {
  // Add the profile to use for verification.
  AddAutofillProfile(_personalDataManager);

  // Bring up the keyboard by tapping the city, which is the element before the
  // picker.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementCity)];

  // Verify the profiles icon is visible.
  [[EarlGrey selectElementWithMatcher:FormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
  [[EarlGrey selectElementWithMatcher:ProfilesIconMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Open a tab in incognito.
  [ChromeEarlGrey openNewIncognitoTab];
  const GURL URL = self.testServer->GetURL(kFormHTMLFile);
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"Profile form"];

  // Bring up the keyboard by tapping the city, which is the element before the
  // picker.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementCity)];

  // Open a  regular tab.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"Profile form"];

  // Bring up the keyboard by tapping the city, which is the element before the
  // picker.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementCity)];

  // This will fail if there is more than one profiles icon in the hierarchy.
  [[EarlGrey selectElementWithMatcher:FormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
  [[EarlGrey selectElementWithMatcher:ProfilesIconMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

#pragma mark - Utilities

// Waits for the passed matcher to be visible with a given timeout.
- (void)waitForMatcherToBeVisible:(id<GREYMatcher>)matcher
                          timeout:(CFTimeInterval)timeout {
  [[GREYCondition conditionWithName:@"Wait for visible matcher condition"
                              block:^BOOL {
                                NSError* error;
                                [[EarlGrey selectElementWithMatcher:matcher]
                                    assertWithMatcher:grey_sufficientlyVisible()
                                                error:&error];
                                return error == nil;
                              }] waitWithTimeout:timeout];
}

@end
