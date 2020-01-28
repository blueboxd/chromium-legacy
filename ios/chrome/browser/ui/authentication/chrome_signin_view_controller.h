// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_CHROME_SIGNIN_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_CHROME_SIGNIN_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#include <memory>

#include "base/auto_reset.h"
#include "base/timer/timer.h"
#import "ios/chrome/browser/signin/constants.h"

@protocol ApplicationCommands;
@protocol BrowsingDataCommands;
class Browser;
class ChromeBrowserState;
@class ChromeIdentity;
@class ChromeSigninViewController;

namespace signin_metrics {
enum class AccessPoint;
enum class PromoAction;
}

using TimerGeneratorBlock = std::unique_ptr<base::OneShotTimer> (^)();

@protocol ChromeSigninViewControllerDelegate<NSObject>

// Informs the delegate that a sign-in operation will start in |controller|.
- (void)willStartSignIn:(ChromeSigninViewController*)controller;

// Informs the delegate that an add account operation will start in
// |controller|.
- (void)willStartAddAccount:(ChromeSigninViewController*)controller;

// Informs the delegate that the user skipped sign-in in |controller|.
- (void)didSkipSignIn:(ChromeSigninViewController*)controller;

// Informs the delegate that the sign-in failed in |controller|.
- (void)didFailSignIn:(ChromeSigninViewController*)controller;

// Informs the delegate that the user was signed in in |controller|.
//
// Note that the sign-in flow did not actually finish when this method is
// called. The delegate will be called with [-didAcceptSignIn:] or
// [-didUndoSignIn:identity:] once the user decides to accept or to undo the
// sign-in operation.
- (void)didSignIn:(ChromeSigninViewController*)controller;

// Informs the delegate that the sign-in of |identity| was undone in
// |controller|.
// |identity| will be automatically forgotten if it was added during this
// sign-in flow.
- (void)didUndoSignIn:(ChromeSigninViewController*)controller
             identity:(ChromeIdentity*)identity;

// Informs the delegate that the user has accepted the sign-in in |controller|.
// If |showAccountSettings| is YES, the delegate is expected to cause the
// account settings to be shown.
// This marks the end of the sign-in flow.
- (void)didAcceptSignIn:(ChromeSigninViewController*)controller
    showAccountsSettings:(BOOL)showAccountsSettings;

@end

// ChromeSigninViewController is a view controller that handles all the
// sign-in UI flow. To support the swipe to dismiss feature, the init method of
// this class uses the presentation controller. Therefore the presentation style
// cannot be changed after the init. The style is set to
// UIModalPresentationFormSheet by the init method.
@interface ChromeSigninViewController : UIViewController

@property(nonatomic, weak) id<ChromeSigninViewControllerDelegate> delegate;

// Data clearing policy to use during the authentication.
// It is valid to set this in the |willStartSignIn:| method of the delegate.
@property(nonatomic, assign) ShouldClearData shouldClearData;

@property(nonatomic, weak, readonly)
    id<ApplicationCommands, BrowsingDataCommands>
        dispatcher;

// Designated initializer.
// * |browser| is the browser where sign-in is being presented.
// * |accessPoint| represents the access point that initiated the sign-in.
// * |identity| will be signed in without requiring user input if not nil.
// * |dispatcher| is the dispatcher that can accept commands for displaying
//   settings views.
- (instancetype)initWithBrowser:(Browser*)browser
                    accessPoint:(signin_metrics::AccessPoint)accessPoint
                    promoAction:(signin_metrics::PromoAction)promoAction
                 signInIdentity:(ChromeIdentity*)identity
                     dispatcher:(id<ApplicationCommands, BrowsingDataCommands>)
                                    dispatcher;

// Cancels the on-going authentication operation (if any). |delegate| will be
// called with |didFailSignIn|.
- (void)cancel;

@end

@interface ChromeSigninViewController (Subclassing)

@property(nonatomic, readonly) Browser* browser;
@property(nonatomic, readonly) ChromeBrowserState* browserState;

// Vertical padding used underneath buttons. Default value is 18.
@property(nonatomic, assign) CGFloat buttonVerticalPadding;

// Primary button title used to accept the sign-in.
@property(nonatomic, readonly) NSString* acceptSigninButtonTitle;

// Secondary button title used to skip the sign-in.
@property(nonatomic, readonly) NSString* skipSigninButtonTitle;

@property(nonatomic, readonly) UIColor* backgroundColor;
@property(nonatomic, readonly) UIButton* primaryButton;
@property(nonatomic, readonly) UIButton* secondaryButton;

@end

// Exposes methods for testing.
@interface ChromeSigninViewController (Testing)

// Timer generator. Should stay nil to use the default timer class:
// base::OneShotTimer.
@property(nonatomic, copy) TimerGeneratorBlock timerGenerator;

// If |shown| is set to NO, activity indicator is not visible while sign-in is
// in progress. This method is needed for EarlGrey tests.
+ (void)setActivityIndicatorShownForTesting:(BOOL)shown;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_CHROME_SIGNIN_VIEW_CONTROLLER_H_
