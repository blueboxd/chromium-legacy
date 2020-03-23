// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/promos/signin_promo_view_controller.h"

#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/sys_string_conversions.h"
#include "base/version.h"
#include "components/signin/ios/browser/features.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#include "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_utils.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_signin_constants.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"
#include "net/base/network_change_notifier.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Name of the UMA SSO Recall histogram.
const char* const kUMASSORecallPromoAction = "SSORecallPromo.PromoAction";

// Name of the histogram recording how many accounts were available on the
// device when the promo was shown.
const char* const kUMASSORecallAccountsAvailable =
    "SSORecallPromo.AccountsAvailable";

// Name of the histogram recording how many times the promo has been shown.
const char* const kUMASSORecallPromoSeenCount = "SSORecallPromo.PromoSeenCount";

// Values of the UMA SSORecallPromo.PromoAction histogram.
enum PromoAction {
  ACTION_DISMISSED,
  ACTION_ENABLED_SSO_ACCOUNT,
  ACTION_ADDED_ANOTHER_ACCOUNT,
  PROMO_ACTION_COUNT
};
}  // namespace

@interface SigninPromoViewController ()<ChromeSigninViewControllerDelegate>
@end

@implementation SigninPromoViewController {
  BOOL _addAccountOperation;
}

- (instancetype)initWithBrowser:(Browser*)browser
                     dispatcher:(id<ApplicationCommands, BrowsingDataCommands>)
                                    dispatcher {
  self = [super
      initWithBrowser:browser
          accessPoint:signin_metrics::AccessPoint::ACCESS_POINT_SIGNIN_PROMO
          promoAction:signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO
       signInIdentity:nil
           dispatcher:dispatcher];
  if (self) {
    super.delegate = self;
  }
  return self;
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];

  if ([self isBeingPresented] || [self isMovingToParentViewController]) {
    signin_metrics::LogSigninAccessPointStarted(
        signin_metrics::AccessPoint::ACCESS_POINT_SIGNIN_PROMO,
        signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO);
    signin_metrics::RecordSigninUserActionForAccessPoint(
        signin_metrics::AccessPoint::ACCESS_POINT_SIGNIN_PROMO,
        signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO);
  }

  [self recordPromoDisplayed];
}

- (void)dismissWithSignedIn:(BOOL)signedIn
       showAccountsSettings:(BOOL)showAccountsSettings {
  DCHECK(self.presentingViewController);
  ProceduralBlock completion = nil;
  if (showAccountsSettings) {
    __weak UIViewController* presentingViewController =
        self.presentingViewController;
    __weak id<ApplicationCommands> dispatcher = self.dispatcher;
    completion = ^{
      [dispatcher showAdvancedSigninSettingsFromViewController:
                      presentingViewController];
    };
  }
  [self.presentingViewController dismissViewControllerAnimated:YES
                                                    completion:completion];
}

// Records in user defaults that the promo has been shown as well as the number
// of times it's been displayed.
// TODO(crbug.com/971989): Move this method to upgrade logger.
- (void)recordPromoDisplayed {
  SigninRecordVersionSeen();
  NSUserDefaults* standardDefaults = [NSUserDefaults standardUserDefaults];
  int promoSeenCount =
      [standardDefaults integerForKey:kDisplayedSSORecallPromoCountKey];
  promoSeenCount++;
  [standardDefaults setInteger:promoSeenCount
                        forKey:kDisplayedSSORecallPromoCountKey];

  NSArray* identities = ios::GetChromeBrowserProvider()
                            ->GetChromeIdentityService()
                            ->GetAllIdentitiesSortedForDisplay();
  UMA_HISTOGRAM_COUNTS_100(kUMASSORecallAccountsAvailable, [identities count]);
  UMA_HISTOGRAM_COUNTS_100(kUMASSORecallPromoSeenCount, promoSeenCount);
}

#pragma mark Superclass overrides

- (UIColor*)backgroundColor {
  return [UIColor colorNamed:kBackgroundColor];
}

#pragma mark - ChromeSigninViewControllerDelegate

- (void)willStartSignIn:(ChromeSigninViewController*)controller {
  DCHECK_EQ(self, controller);
  _addAccountOperation = NO;
}

- (void)willStartAddAccount:(ChromeSigninViewController*)controller {
  DCHECK_EQ(self, controller);
  _addAccountOperation = YES;
}

- (void)didSkipSignIn:(ChromeSigninViewController*)controller {
  DCHECK_EQ(self, controller);
  UMA_HISTOGRAM_ENUMERATION(kUMASSORecallPromoAction, ACTION_DISMISSED,
                            PROMO_ACTION_COUNT);
  [self dismissWithSignedIn:NO showAccountsSettings:NO];
}

- (void)didFailSignIn:(ChromeSigninViewController*)controller {
  DCHECK_EQ(self, controller);
  [self dismissWithSignedIn:NO showAccountsSettings:NO];
}

- (void)didSignIn:(ChromeSigninViewController*)controller {
  DCHECK_EQ(self, controller);
}

- (void)didUndoSignIn:(ChromeSigninViewController*)controller
             identity:(ChromeIdentity*)identity {
  DCHECK_EQ(self, controller);
  // No accounts case is impossible in SigninPromoViewController, nothing to do.
}

- (void)didAcceptSignIn:(ChromeSigninViewController*)controller
    showAccountsSettings:(BOOL)showAccountsSettings {
  DCHECK_EQ(self, controller);
  PromoAction promoAction = _addAccountOperation ? ACTION_ADDED_ANOTHER_ACCOUNT
                                                 : ACTION_ENABLED_SSO_ACCOUNT;
  UMA_HISTOGRAM_ENUMERATION(kUMASSORecallPromoAction, promoAction,
                            PROMO_ACTION_COUNT);

  [self dismissWithSignedIn:YES showAccountsSettings:showAccountsSettings];
}

@end
