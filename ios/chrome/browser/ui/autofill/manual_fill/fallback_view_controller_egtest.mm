// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#include <atomic>

#include "base/ios/ios_util.h"
#import "base/test/ios/wait_util.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/ios/browser/autofill_switches.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
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

using chrome_test_util::TapWebElementWithId;

namespace {

constexpr char kFormElementNormal[] = "normal_field";
constexpr char kFormElementReadonly[] = "readonly_field";

constexpr char kFormHTMLFile[] = "/readonly_form.html";

// Returns a matcher for the profiles icon in the keyboard accessory bar.
id<GREYMatcher> ProfilesIconMatcher() {
  return grey_accessibilityID(
      manual_fill::AccessoryAddressAccessibilityIdentifier);
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

}  // namespace

// Integration Tests for fallback coordinator.
@interface FallbackViewControllerTestCase : ChromeTestCase {
  // The PersonalDataManager instance for the current browser state.
  autofill::PersonalDataManager* _personalDataManager;
}

@end

@implementation FallbackViewControllerTestCase

- (void)setUp {
  [super setUp];
  ios::ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();
  _personalDataManager =
      autofill::PersonalDataManagerFactory::GetForBrowserState(browserState);
  _personalDataManager->SetSyncingForTest(true);
  _personalDataManager->ClearAllLocalData();
  AddAutofillProfile(_personalDataManager);

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL URL = self.testServer->GetURL(kFormHTMLFile);
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"Hello"];
}

- (void)tearDown {
  _personalDataManager->ClearAllLocalData();
  [super tearDown];
}

// Tests that readonly fields don't have Manual Fallback icons.
- (void)testReadOnlyFieldDoesNotShowManualFallbackIcons {
  // Tap the readonly field.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementReadonly)];

  // Verify the profiles icon is not visible.
  [[EarlGrey selectElementWithMatcher:ProfilesIconMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that readonly fields don't have Manual Fallback icons after tapping a
// regular field.
- (void)testReadOnlyFieldDoesNotShowManualFallbackIconsAfterNormalField {
  // Tap the regular field.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementNormal)];

  // Verify the profiles icon is visible.
  [[EarlGrey selectElementWithMatcher:ProfilesIconMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the readonly field.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementReadonly)];

  // Verify the profiles icon is not visible.
  [[EarlGrey selectElementWithMatcher:ProfilesIconMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that normal fields have Manual Fallback icons after tapping a readonly
// field.
- (void)testNormalFieldHasManualFallbackIconsAfterReadonlyField {
  // Tap the readonly field.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementReadonly)];

  // Verify the profiles icon is not visible.
  [[EarlGrey selectElementWithMatcher:ProfilesIconMatcher()]
      assertWithMatcher:grey_notVisible()];

  // Tap the regular field.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementNormal)];

  // Verify the profiles icon is visible.
  [[EarlGrey selectElementWithMatcher:ProfilesIconMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
