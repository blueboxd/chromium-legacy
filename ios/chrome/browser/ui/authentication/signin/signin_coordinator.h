// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SIGNIN_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SIGNIN_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_enums.h"
#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

class Browser;
@class ChromeIdentity;

@protocol ApplicationCommands;
@protocol BrowsingDataCommands;
@protocol SyncPresenter;

// Called when the sign-in dialog is closed.
// |result| is the sign-in result state.
// |identity| is the identity chosen by the user during the sign-in.
typedef void (^SigninCoordinatorCompletionCallback)(
    SigninCoordinatorResult signinResult,
    ChromeIdentity* identity);

// Main class for sign-in coordinator. This class should not be instantiated
// directly, this should be done using the class methods.
@interface SigninCoordinator : ChromeCoordinator

// Dispatcher.
@property(nonatomic, strong, readonly)
    id<ApplicationCommands, BrowsingDataCommands>
        dispatcher;

// Called when the sign-in dialog is interrupted, canceled or successful.
@property(nonatomic, copy) SigninCoordinatorCompletionCallback signinCompletion;

// Returns a coordinator for user sign-in workflow.
// |viewController| presents the sign-in.
// |identity| is the identity preselected with the sign-in opens.
// |accessPoint| is the view where the sign-in button was displayed.
// |promoAction| is promo button used to trigger the sign-in.
+ (instancetype)
    userSigninCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                        browser:(Browser*)browser
                                       identity:(ChromeIdentity*)identity
                                    accessPoint:
                                        (signin_metrics::AccessPoint)accessPoint
                                    promoAction:(signin_metrics::PromoAction)
                                                    promoAction;

// Returns a coordinator for first run sign-in workflow.
// |viewController| presents the sign-in.
// |presenter| to present sync-related UI.
+ (instancetype)
    firstRunCoordinatorWithBaseViewController:(UIViewController*)viewController
                                      browser:(Browser*)browser
                                syncPresenter:(id<SyncPresenter>)presenter;

// Returns a coordinator for upgrade sign-in workflow.
// |viewController| presents the sign-in.
+ (instancetype)upgradeSigninPromoCoordinatorWithBaseViewController:
                    (UIViewController*)viewController
                                                            browser:(Browser*)
                                                                        browser;

// Returns a coordinator for advanced sign-in settings workflow.
// |viewController| presents the sign-in.
+ (instancetype)
    advancedSettingsSigninCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                                    browser:(Browser*)browser;

// Returns a coordinator to add an account.
// |viewController| presents the sign-in.
// |accessPoint| access point from the sign-in where is started.
+ (instancetype)
    addAccountCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                        browser:(Browser*)browser
                                    accessPoint:(signin_metrics::AccessPoint)
                                                    accessPoint;

// Returns a coordinator for re-authentication workflow.
// |viewController| presents the sign-in.
// |accessPoint| access point from the sign-in where is started.
// |promoAction| is promo button used to trigger the sign-in.
+ (instancetype)
    reAuthenticationCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                              browser:(Browser*)browser
                                          accessPoint:
                                              (signin_metrics::AccessPoint)
                                                  accessPoint
                                          promoAction:
                                              (signin_metrics::PromoAction)
                                                  promoAction;

// Interrupts the sign-in flow.
// |action| action describing how to interrupt the sign-in.
// |completion| called once the sign-in is fully interrupted.
- (void)interruptWithAction:(SigninCoordinatorInterruptAction)action
                 completion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SIGNIN_COORDINATOR_H_
