// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui.h"

#import <EarlGrey/EarlGrey.h>

#include "base/mac/foundation_util.h"
#include "components/unified_consent/feature.h"
#include "ios/chrome/browser/ui/authentication/signin_confirmation_view_controller.h"
#import "ios/chrome/browser/ui/authentication/signin_earlgrey_utils.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_cell.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_picker_view.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_view_controller.h"
#import "ios/chrome/browser/ui/util/transparent_link_button.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Returns a matcher to test whether the element is a scroll view with a content
// smaller than the scroll view bounds.
id<GREYMatcher> ContentViewSmallerThanScrollView() {
  MatchesBlock matches = ^BOOL(UIView* view) {
    UIScrollView* scrollView = base::mac::ObjCCast<UIScrollView>(view);
    return scrollView &&
           scrollView.contentSize.height < scrollView.bounds.size.height;
  };
  DescribeToBlock describe = ^void(id<GREYDescription> description) {
    [description appendText:
                     @"Not a scroll view or the scroll view content is bigger "
                     @"than the scroll view bounds"];
  };
  return [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                              descriptionBlock:describe];
}

}  // namespace

using chrome_test_util::AccountConsistencyConfirmationOkButton;
using chrome_test_util::AccountConsistencySetupSigninButton;
using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::PrimarySignInButton;
using chrome_test_util::SecondarySignInButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::UnifiedConsentAddAccountButton;

@implementation SigninEarlGreyUI

+ (void)signinWithIdentity:(ChromeIdentity*)identity {
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::SecondarySignInButton()];
  [self selectIdentityWithEmail:identity.userEmail];
  [self confirmSigninConfirmationDialog];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
  [SigninEarlGreyUtils checkSignedInWithIdentity:identity];
}

+ (void)selectIdentityWithEmail:(NSString*)userEmail {
  if (unified_consent::IsUnifiedConsentFeatureEnabled()) {
    // Assumes that the identity chooser is visible.
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(grey_accessibilityID(userEmail),
                                            grey_kindOfClass(
                                                [IdentityChooserCell class]),
                                            grey_sufficientlyVisible(), nil)]
        performAction:grey_tap()];
  } else {
    // Sign in to |userEmail|.
    [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(userEmail)]
        performAction:grey_tap()];
    [[EarlGrey selectElementWithMatcher:AccountConsistencySetupSigninButton()]
        performAction:grey_tap()];
  }
}

+ (void)tapSettingsLink {
  id<GREYMatcher> settingsLinkMatcher =
      grey_allOf(grey_kindOfClass([TransparentLinkButton class]),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:settingsLinkMatcher]
      performAction:grey_tap()];
}

+ (void)confirmSigninConfirmationDialog {
  // To confirm the dialog, the scroll view content has to be scrolled to the
  // bottom to transform "MORE" button into the validation button.
  // EarlGrey fails to scroll to the bottom, using grey_scrollToContentEdge(),
  // if the scroll view doesn't bounce and by default a scroll view doesn't
  // bounce when the content fits into the scroll view (the scroll never ends).
  // To test if the content fits into the scroll view,
  // ContentViewSmallerThanScrollView() matcher is used on the signin scroll
  // view.
  // If the matcher fails, then the scroll view should be scrolled to the
  // bottom.
  // Once to the bottom, the consent can be confirmed.
  id<GREYMatcher> confirmationScrollViewMatcher = nil;
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  if (unified_consent::IsUnifiedConsentFeatureEnabled()) {
    confirmationScrollViewMatcher =
        grey_accessibilityID(kUnifiedConsentScrollViewIdentifier);
  } else {
    confirmationScrollViewMatcher = grey_allOf(
        grey_ancestor(
            grey_accessibilityID(kSigninConfirmationCollectionViewId)),
        grey_kindOfClass([UICollectionView class]), nil);
  }
  NSError* error = nil;
  [[EarlGrey selectElementWithMatcher:confirmationScrollViewMatcher]
      assertWithMatcher:ContentViewSmallerThanScrollView()
                  error:&error];
  if (error) {
    // If the consent is bigger than the scroll view, the primary button should
    // be "MORE".
    [[EarlGrey selectElementWithMatcher:
                   chrome_test_util::ButtonWithAccessibilityLabelId(
                       IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_SCROLL_BUTTON)]
        assertWithMatcher:grey_notNil()];
    [[EarlGrey selectElementWithMatcher:confirmationScrollViewMatcher]
        performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
  }
  [[EarlGrey selectElementWithMatcher:AccountConsistencyConfirmationOkButton()]
      performAction:grey_tap()];
}

+ (void)tapAddAccountButton {
  GREYAssertTrue(unified_consent::IsUnifiedConsentFeatureEnabled(),
                 @"-[SigninEarlGreyUI tapAddAccountButton] is not available "
                 @"without UnifiedConsent flag");
  id<GREYMatcher> confirmationScrollViewMatcher =
      grey_accessibilityID(kUnifiedConsentScrollViewIdentifier);
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  NSError* error = nil;
  [[EarlGrey selectElementWithMatcher:confirmationScrollViewMatcher]
      assertWithMatcher:ContentViewSmallerThanScrollView()
                  error:&error];
  if (error) {
    // If the consent is bigger than the scroll view, the primary button should
    // be "MORE".
    [[EarlGrey selectElementWithMatcher:
                   chrome_test_util::ButtonWithAccessibilityLabelId(
                       IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_SCROLL_BUTTON)]
        assertWithMatcher:grey_notNil()];
    [[EarlGrey selectElementWithMatcher:confirmationScrollViewMatcher]
        performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
  }
  [[EarlGrey selectElementWithMatcher:UnifiedConsentAddAccountButton()]
      performAction:grey_tap()];
}

+ (void)checkSigninPromoVisibleWithMode:(SigninPromoViewMode)mode {
  [self checkSigninPromoVisibleWithMode:mode closeButton:YES];
}

+ (void)checkSigninPromoVisibleWithMode:(SigninPromoViewMode)mode
                            closeButton:(BOOL)closeButton {
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(kSigninPromoViewId),
                                   grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
  switch (mode) {
    case SigninPromoViewModeColdState:
      [[EarlGrey
          selectElementWithMatcher:grey_allOf(SecondarySignInButton(),
                                              grey_sufficientlyVisible(), nil)]
          assertWithMatcher:grey_nil()];
      break;
    case SigninPromoViewModeWarmState:
      [[EarlGrey
          selectElementWithMatcher:grey_allOf(SecondarySignInButton(),
                                              grey_sufficientlyVisible(), nil)]
          assertWithMatcher:grey_notNil()];
      break;
  }
  if (closeButton) {
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                                kSigninPromoCloseButtonId),
                                            grey_sufficientlyVisible(), nil)]
        assertWithMatcher:grey_notNil()];
  }
}

+ (void)checkSigninPromoNotVisible {
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(kSigninPromoViewId),
                                   grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(SecondarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
}

@end
