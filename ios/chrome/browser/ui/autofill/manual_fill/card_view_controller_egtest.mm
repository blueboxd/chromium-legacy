// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <EarlGrey/GREYKeyboard.h>

#include "base/ios/ios_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/keyed_service/core/service_access_type.h"
#import "ios/chrome/browser/autofill/form_suggestion_constants.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/card_coordinator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/card_mediator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/card_view_controller.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_accessory_view_controller.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_credit_card_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/autofill/features.h"
#import "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
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
using chrome_test_util::StaticTextWithAccessibilityLabelId;
using chrome_test_util::TapWebElementWithId;

namespace {

const char kFormElementUsername[] = "username";
const char kFormElementOtherStuff[] = "otherstuff";

NSString* kLocalCardNumber = @"4111111111111111";
NSString* kLocalCardHolder = @"Test User";
NSString* kLocalCardExpirationMonth = @"11";
NSString* kLocalCardExpirationYear = @"22";

// Unicode characters used in card number:
//  - 0x0020 - Space.
//  - 0x2060 - WORD-JOINER (makes string undivisible).
constexpr base::char16 separator[] = {0x2060, 0x0020, 0};
NSString* kObfuscatedNumberPrefix = base::SysUTF16ToNSString(
    autofill::kMidlineEllipsis + base::string16(separator) +
    autofill::kMidlineEllipsis + base::string16(separator) +
    autofill::kMidlineEllipsis + base::string16(separator));

NSString* kLocalNumberObfuscated =
    [NSString stringWithFormat:@"%@1111", kObfuscatedNumberPrefix];

NSString* kServerNumberObfuscated =
    [NSString stringWithFormat:@"%@2109", kObfuscatedNumberPrefix];

const char kFormHTMLFile[] = "/multi_field_form.html";

// Timeout in seconds while waiting for a profile or a credit to be added to the
// PersonalDataManager.
const NSTimeInterval kPDMMaxDelaySeconds = 10.0;

// Returns a matcher for the scroll view in keyboard accessory bar.
id<GREYMatcher> FormSuggestionViewMatcher() {
  return grey_accessibilityID(kFormSuggestionsViewAccessibilityIdentifier);
}

// Returns a matcher for the credit card icon in the keyboard accessory bar.
id<GREYMatcher> CreditCardIconMatcher() {
  return grey_accessibilityID(
      manual_fill::AccessoryCreditCardAccessibilityIdentifier);
}

id<GREYMatcher> KeyboardIconMatcher() {
  return grey_accessibilityID(
      manual_fill::AccessoryKeyboardAccessibilityIdentifier);
}

// Returns a matcher for the credit card table view in manual fallback.
id<GREYMatcher> CreditCardTableViewMatcher() {
  return grey_accessibilityID(
      manual_fill::CardTableViewAccessibilityIdentifier);
}

// Returns a matcher for the button to open password settings in manual
// fallback.
id<GREYMatcher> ManageCreditCardsMatcher() {
  return grey_accessibilityID(manual_fill::ManageCardsAccessibilityIdentifier);
}

// Returns a matcher for the button to add credit cards settings in manual
// fallback.
id<GREYMatcher> AddCreditCardsMatcher() {
  return grey_accessibilityID(
      manual_fill::kAddCreditCardsAccessibilityIdentifier);
}

// Returns a matcher for the credit card settings collection view.
id<GREYMatcher> CreditCardSettingsMatcher() {
  return grey_accessibilityID(kAutofillCreditCardTableViewId);
}

// Matcher for the not secure website alert.
id<GREYMatcher> NotSecureWebsiteAlert() {
  return StaticTextWithAccessibilityLabelId(
      IDS_IOS_MANUAL_FALLBACK_NOT_SECURE_TITLE);
}

// Returns a matcher for the CreditCardTableView window.
id<GREYMatcher> CreditCardTableViewWindowMatcher() {
  id<GREYMatcher> classMatcher = grey_kindOfClass([UIWindow class]);
  id<GREYMatcher> parentMatcher = grey_descendant(CreditCardTableViewMatcher());
  return grey_allOf(classMatcher, parentMatcher, nil);
}

// Returns the matcher for an enabled cancel button in a navigation bar.
id<GREYMatcher> NavigationBarCancelMatcher() {
  return grey_allOf(
      grey_ancestor(grey_kindOfClass([UINavigationBar class])), CancelButton(),
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitNotEnabled)), nil);
}

// Polls the JavaScript query |java_script_condition| until the returned
// |boolValue| is YES with a kWaitForActionTimeout timeout.
BOOL WaitForJavaScriptCondition(NSString* java_script_condition) {
  auto verify_block = ^BOOL {
    id value = chrome_test_util::ExecuteJavaScript(java_script_condition, nil);
    return [value isEqual:@YES];
  };
  NSTimeInterval timeout = base::test::ios::kWaitForActionTimeout;
  NSString* condition_name = [NSString
      stringWithFormat:@"Wait for JS condition: %@", java_script_condition];
  GREYCondition* condition = [GREYCondition conditionWithName:condition_name
                                                        block:verify_block];
  return [condition waitWithTimeout:timeout];
}

}  // namespace

// Integration Tests for Manual Fallback credit cards View Controller.
@interface CreditCardViewControllerTestCase : ChromeTestCase {
  // The PersonalDataManager instance for the current browser state.
  autofill::PersonalDataManager* _personalDataManager;

  // Credit Cards added to the PersonalDataManager.
  std::vector<autofill::CreditCard> _cards;
}
@end

@implementation CreditCardViewControllerTestCase

- (void)setUp {
  [super setUp];
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL URL = self.testServer->GetURL(kFormHTMLFile);
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"hello!"];

  _personalDataManager =
      autofill::PersonalDataManagerFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  _personalDataManager->SetSyncingForTest(true);

  for (autofill::CreditCard* card :
       _personalDataManager->GetLocalCreditCards()) {
    _personalDataManager->RemoveByGUID(card->guid());
  }
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 kPDMMaxDelaySeconds,
                 ^bool() {
                   return _personalDataManager->GetLocalCreditCards().size() ==
                          0;
                 }),
             @"Failed to remove local credit cards.");
}

- (void)tearDown {
  for (const auto& card : _cards) {
    _personalDataManager->RemoveByGUID(card.guid());
  }
  _cards.clear();

  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait
                           errorOrNil:nil];
  [super tearDown];
}

#pragma mark - Helpers

// Adds a local credit card, one that doesn't need CVC unlocking.
- (void)addCreditCard:(const autofill::CreditCard&)card {
  _cards.push_back(card);
  size_t card_count = _personalDataManager->GetCreditCards().size();
  _personalDataManager->AddCreditCard(card);
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 kPDMMaxDelaySeconds,
                 ^bool() {
                   return card_count <
                          _personalDataManager->GetCreditCards().size();
                 }),
             @"Failed to add credit card.");
  _personalDataManager->NotifyPersonalDataObserver();
}

// Adds a server credit card, one that needs CVC unlocking.
- (void)addServerCreditCard:(const autofill::CreditCard&)card {
  DCHECK(card.record_type() != autofill::CreditCard::LOCAL_CARD);
  _personalDataManager->AddServerCreditCardForTest(
      std::make_unique<autofill::CreditCard>(card));
  _personalDataManager->NotifyPersonalDataObserver();
}

- (void)saveLocalCreditCard {
  autofill::CreditCard card = autofill::test::GetCreditCard();
  [self addCreditCard:card];
}

- (void)saveMaskedCreditCard {
  autofill::CreditCard card = autofill::test::GetMaskedServerCard();
  [self addServerCreditCard:card];
}

#pragma mark - Tests

// Tests that the credit card view butotn is absent when there are no cards
// available.
- (void)testCreditCardsButtonAbsentWhenNoCreditCardsAvailable {
  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Verify there's no credit card icon.
  [[EarlGrey selectElementWithMatcher:CreditCardIconMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the credit card view controller appears on screen.
- (void)testCreditCardsViewControllerIsPresented {
  [self saveLocalCreditCard];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the credit card icon.
  [[EarlGrey selectElementWithMatcher:CreditCardIconMatcher()]
      performAction:grey_tap()];

  // Verify the credit cards controller table view is visible.
  [[EarlGrey selectElementWithMatcher:CreditCardTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the credit cards view controller contains the "Manage Credit
// Cards..." action.
- (void)testCreditCardsViewControllerContainsManageCreditCardsAction {
  [self saveLocalCreditCard];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the credit card icon.
  [[EarlGrey selectElementWithMatcher:CreditCardIconMatcher()]
      performAction:grey_tap()];

  // Try to scroll.
  [[EarlGrey selectElementWithMatcher:CreditCardTableViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  // Verify the credit cards controller contains the "Manage Credit Cards..."
  // action.
  [[EarlGrey selectElementWithMatcher:ManageCreditCardsMatcher()]
      assertWithMatcher:grey_interactable()];
}

// Tests that the "Manage Credit Cards..." action works.
- (void)testManageCreditCardsActionOpensCreditCardSettings {
  [self saveLocalCreditCard];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the credit card icon.
  [[EarlGrey selectElementWithMatcher:CreditCardIconMatcher()]
      performAction:grey_tap()];

  // Try to scroll.
  [[EarlGrey selectElementWithMatcher:CreditCardTableViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  // Tap the "Manage Credit Cards..." action.
  [[EarlGrey selectElementWithMatcher:ManageCreditCardsMatcher()]
      performAction:grey_tap()];

  // Verify the credit cards settings opened.
  [[EarlGrey selectElementWithMatcher:CreditCardSettingsMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the manual fallback view and icon is not highlighted after
// presenting the manage credit cards view.
- (void)testCreditCardsStateAfterPresentingCreditCardSettings {
  [self saveLocalCreditCard];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Scroll to the right.
  [[EarlGrey selectElementWithMatcher:FormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];

  // Tap on the credit card icon.
  [[EarlGrey selectElementWithMatcher:CreditCardIconMatcher()]
      performAction:grey_tap()];

  // Verify the status of the icon.
  [[EarlGrey selectElementWithMatcher:CreditCardIconMatcher()]
      assertWithMatcher:grey_not(grey_userInteractionEnabled())];

  // Try to scroll.
  [[EarlGrey selectElementWithMatcher:CreditCardTableViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  // Tap the "Manage Credit Cards..." action.
  [[EarlGrey selectElementWithMatcher:ManageCreditCardsMatcher()]
      performAction:grey_tap()];

  // Tap Cancel Button.
  [[EarlGrey selectElementWithMatcher:NavigationBarCancelMatcher()]
      performAction:grey_tap()];

  // Verify the status of the icons.
  [[EarlGrey selectElementWithMatcher:CreditCardIconMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:CreditCardIconMatcher()]
      assertWithMatcher:grey_userInteractionEnabled()];
  [[EarlGrey selectElementWithMatcher:KeyboardIconMatcher()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  // Verify the keyboard is not cover by the cards view.
  [[EarlGrey selectElementWithMatcher:CreditCardTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the "Add Credit Cards..." action works.
- (void)testAddCreditCardsActionOpensAddCreditCardSettings {
  base::test::ScopedFeatureList featureList;
  featureList.InitAndEnableFeature(kSettingsAddPaymentMethod);
  [self saveLocalCreditCard];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the credit card icon.
  [[EarlGrey selectElementWithMatcher:CreditCardIconMatcher()]
      performAction:grey_tap()];

  // Try to scroll.
  [[EarlGrey selectElementWithMatcher:CreditCardTableViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  // Tap the "Add Credit Cards..." action.
  [[EarlGrey selectElementWithMatcher:AddCreditCardsMatcher()]
      performAction:grey_tap()];

  // Verify the credit cards settings opened.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::AddCreditCardView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the "Add Credit Cards..." action works on OTR.
- (void)testOTRAddCreditCardsActionOpensAddCreditCardSettings {
  base::test::ScopedFeatureList featureList;
  featureList.InitAndEnableFeature(kSettingsAddPaymentMethod);
  [self saveLocalCreditCard];

  // Open a tab in incognito.
  [ChromeEarlGrey openNewIncognitoTab];
  const GURL URL = self.testServer->GetURL(kFormHTMLFile);
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"hello!"];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the credit card icon.
  [[EarlGrey selectElementWithMatcher:CreditCardIconMatcher()]
      performAction:grey_tap()];

  // Try to scroll.
  [[EarlGrey selectElementWithMatcher:CreditCardTableViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  // Tap the "Add Credit Cards..." action.
  [[EarlGrey selectElementWithMatcher:AddCreditCardsMatcher()]
      performAction:grey_tap()];

  // Verify the credit cards settings opened.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::AddCreditCardView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the manual fallback view icon is not highlighted after presenting
// the add credit card view.
- (void)testCreditCardsButtonStateAfterPresentingAddCreditCard {
  base::test::ScopedFeatureList featureList;
  featureList.InitAndEnableFeature(kSettingsAddPaymentMethod);
  [self saveLocalCreditCard];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Scroll to the right.
  [[EarlGrey selectElementWithMatcher:FormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];

  // Tap on the credit card icon.
  [[EarlGrey selectElementWithMatcher:CreditCardIconMatcher()]
      performAction:grey_tap()];

  // Verify the status of the icon.
  [[EarlGrey selectElementWithMatcher:CreditCardIconMatcher()]
      assertWithMatcher:grey_not(grey_userInteractionEnabled())];

  // Try to scroll.
  [[EarlGrey selectElementWithMatcher:CreditCardTableViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  // Tap the "Add Credit Cards..." action.
  [[EarlGrey selectElementWithMatcher:AddCreditCardsMatcher()]
      performAction:grey_tap()];

  // Tap Cancel Button.
  [[EarlGrey selectElementWithMatcher:NavigationBarCancelMatcher()]
      performAction:grey_tap()];

  // Verify the status of the icons.
  [[EarlGrey selectElementWithMatcher:CreditCardIconMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:CreditCardIconMatcher()]
      assertWithMatcher:grey_userInteractionEnabled()];
  [[EarlGrey selectElementWithMatcher:KeyboardIconMatcher()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  // Verify the keyboard is not cover by the cards view.
  [[EarlGrey selectElementWithMatcher:CreditCardTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the credit card View Controller is dismissed when tapping the
// keyboard icon.
- (void)testKeyboardIconDismissCreditCardController {
  if ([ChromeEarlGrey isIPadIdiom]) {
    // The keyboard icon is never present in iPads.
    return;
  }
  [self saveLocalCreditCard];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the credit cards icon.
  [[EarlGrey selectElementWithMatcher:CreditCardIconMatcher()]
      performAction:grey_tap()];

  // Verify the credit card controller table view is visible.
  [[EarlGrey selectElementWithMatcher:CreditCardTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on the keyboard icon.
  [[EarlGrey selectElementWithMatcher:KeyboardIconMatcher()]
      performAction:grey_tap()];

  // Verify the credit card controller table view and the credit card icon is
  // NOT visible.
  [[EarlGrey selectElementWithMatcher:CreditCardTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:KeyboardIconMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the credit card View Controller is dismissed when tapping the
// outside the popover on iPad.
- (void)testIPadTappingOutsidePopOverDismissCreditCardController {
  if (![ChromeEarlGrey isIPadIdiom]) {
    return;
  }
  [self saveLocalCreditCard];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the credit cards icon.
  [[EarlGrey selectElementWithMatcher:CreditCardIconMatcher()]
      performAction:grey_tap()];

  // Verify the credit card controller table view is visible.
  [[EarlGrey selectElementWithMatcher:CreditCardTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on a point outside of the popover.
  // The way EarlGrey taps doesn't go through the window hierarchy. Because of
  // this, the tap needs to be done in the same window as the popover.
  [[EarlGrey selectElementWithMatcher:CreditCardTableViewWindowMatcher()]
      performAction:grey_tapAtPoint(CGPointMake(0, 0))];

  // Verify the credit card controller table view and the credit card icon is
  // NOT visible.
  [[EarlGrey selectElementWithMatcher:CreditCardTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:KeyboardIconMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the credit card View Controller is dismissed when tapping the
// keyboard.
// TODO(crbug.com/1017095): Re-enable this test.
- (void)DISABLED_testTappingKeyboardDismissCreditCardControllerPopOver {
  if (![ChromeEarlGrey isIPadIdiom]) {
    return;
  }
  [self saveLocalCreditCard];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the credit cards icon.
  [[EarlGrey selectElementWithMatcher:CreditCardIconMatcher()]
      performAction:grey_tap()];

  // Verify the credit card controller table view is visible.
  [[EarlGrey selectElementWithMatcher:CreditCardTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:CreditCardTableViewMatcher()]
      performAction:grey_typeText(@"text")];

  // Verify the credit card controller table view and the credit card icon is
  // NOT visible.
  [[EarlGrey selectElementWithMatcher:CreditCardTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:KeyboardIconMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that after switching fields the content size of the table view didn't
// grow.
- (void)testCreditCardControllerKeepsRightSize {
  [self saveLocalCreditCard];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the credit cards icon.
  [[EarlGrey selectElementWithMatcher:CreditCardIconMatcher()]
      performAction:grey_tap()];

  // Tap the second element.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementOtherStuff)];

  // Try to scroll.
  [[EarlGrey selectElementWithMatcher:CreditCardTableViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
}

// Tests that the credit card View Controller stays on rotation.
- (void)testCreditCardControllerSupportsRotation {
  [self saveLocalCreditCard];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Tap on the credit cards icon.
  [[EarlGrey selectElementWithMatcher:CreditCardIconMatcher()]
      performAction:grey_tap()];

  // Verify the credit card controller table view is visible.
  [[EarlGrey selectElementWithMatcher:CreditCardTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                           errorOrNil:nil];

  // Verify the credit card controller table view is still visible.
  [[EarlGrey selectElementWithMatcher:CreditCardTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that credit card number (for local card) is injected.
// TODO(crbug.com/845472): maybe figure a way to test successfull injection
// when page is https, but right now if we use the https embedded server,
// there's a warning page that stops the flow because of a
// NET::ERR_CERT_AUTHORITY_INVALID.
- (void)testCreditCardLocalNumberDoesntInjectOnHttp {
  [self verifyCreditCardButtonWithTitle:kLocalNumberObfuscated
                        doesInjectValue:@""];
}

// Tests an alert is shown warning the user when trying to fill a credit card
// number in an HTTP form.
- (void)testCreditCardLocalNumberShowsWarningOnHttp {
  [self verifyCreditCardButtonWithTitle:kLocalNumberObfuscated
                        doesInjectValue:@""];
  // Look for the alert.
  [[EarlGrey selectElementWithMatcher:NotSecureWebsiteAlert()]
      assertWithMatcher:grey_not(grey_nil())];
}

// Tests that credit card cardholder is injected.
- (void)testCreditCardCardHolderInjectsCorrectly {
  [self verifyCreditCardButtonWithTitle:kLocalCardHolder
                        doesInjectValue:kLocalCardHolder];
}

// Tests that credit card expiration month is injected.
- (void)testCreditCardExpirationMonthInjectsCorrectly {
  [self verifyCreditCardButtonWithTitle:kLocalCardExpirationMonth
                        doesInjectValue:kLocalCardExpirationMonth];
}

// Tests that credit card expiration year is injected.
- (void)testCreditCardExpirationYearInjectsCorrectly {
  [self verifyCreditCardButtonWithTitle:kLocalCardExpirationYear
                        doesInjectValue:kLocalCardExpirationYear];
}

// Tests that masked credit card offer CVC input.
// TODOD(crbug.com/909748) can't test this one until https tests are possible.
- (void)DISABLED_testCreditCardServerNumberRequiresCVC {
  [self saveMaskedCreditCard];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Wait for the accessory icon to appear.
  [GREYKeyboard waitForKeyboardToAppear];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:CreditCardIconMatcher()]
      performAction:grey_tap()];

  // Verify the password controller table view is visible.
  [[EarlGrey selectElementWithMatcher:CreditCardTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Select a the masked number.
  [[EarlGrey selectElementWithMatcher:grey_buttonTitle(kServerNumberObfuscated)]
      performAction:grey_tap()];

  // Verify the CVC requester is visible.
  [[EarlGrey selectElementWithMatcher:grey_text(@"Confirm Card")]
      assertWithMatcher:grey_notNil()];

  // TODO(crbug.com/845472): maybe figure a way to enter CVC and get the
  // unlocked card result.
}

#pragma mark - Private

- (void)verifyCreditCardButtonWithTitle:(NSString*)title
                        doesInjectValue:(NSString*)result {
  [self saveLocalCreditCard];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:TapWebElementWithId(kFormElementUsername)];

  // Wait for the accessory icon to appear.
  [GREYKeyboard waitForKeyboardToAppear];

  // Tap on the passwords icon.
  [[EarlGrey selectElementWithMatcher:CreditCardIconMatcher()]
      performAction:grey_tap()];

  // Verify the password controller table view is visible.
  [[EarlGrey selectElementWithMatcher:CreditCardTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Select a field.
  [[EarlGrey selectElementWithMatcher:grey_buttonTitle(title)]
      performAction:grey_tap()];

  // Verify Web Content.
  NSString* javaScriptCondition = [NSString
      stringWithFormat:@"window.document.getElementById('%s').value === '%@'",
                       kFormElementUsername, result];
  XCTAssertTrue(WaitForJavaScriptCondition(javaScriptCondition));
}

@end
