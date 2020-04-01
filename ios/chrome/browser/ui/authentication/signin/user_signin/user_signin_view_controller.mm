// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_signin_view_controller.h"

#import "base/logging.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/gradient_view.h"
#import "ios/chrome/browser/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/ActivityIndicator/src/MaterialActivityIndicator.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Button corner radius.
const CGFloat kButtonCornerRadius = 8;
// Inset between button contents and edge.
const CGFloat kButtonTitleContentInset = 8.0;

// Layout constants for buttons.
struct AuthenticationViewConstants {
  CGFloat GradientHeight;
  CGFloat ButtonHeight;
  CGFloat ButtonHorizontalPadding;
  CGFloat ButtonTopPadding;
  CGFloat ButtonBottomPadding;
};

const AuthenticationViewConstants kCompactConstants = {
    40,  // GradientHeight
    36,  // ButtonHeight
    16,  // ButtonHorizontalPadding
    16,  // ButtonTopPadding
    16,  // ButtonBottomPadding
};

const AuthenticationViewConstants kRegularConstants = {
    kCompactConstants.GradientHeight,
    1.5 * kCompactConstants.ButtonHeight,
    32,  // ButtonHorizontalPadding
    32,  // ButtonTopPadding
    32,  // ButtonBottomPadding
};

// The style applied to a button type.
enum AuthenticationButtonType {
  AuthenticationButtonTypeMore,
  AuthenticationButtonTypeAddAccount,
  AuthenticationButtonTypeConfirmation,
};
}  // namespace

@interface UserSigninViewController ()

// Activity indicator used to block the UI when a sign-in operation is in
// progress.
@property(nonatomic, strong) MDCActivityIndicator* activityIndicator;
// Button used to confirm the sign-in operation, e.g. "Yes I'm In".
@property(nonatomic, strong) UIButton* confirmationButton;
// Button used to exit the sign-in operation without confirmation, e.g. "No
// Thanks", "Cancel".
@property(nonatomic, strong) UIButton* skipSigninButton;
// Property that denotes whether the unified consent screen reached bottom has
// triggered.
@property(nonatomic, assign) BOOL hasUnifiedConsentScreenReachedBottom;
// Gradient used to hide text that is close to the bottom of the screen. This
// gives users the hint that there is more to scroll through.
@property(nonatomic, strong) GradientView* gradientView;

@end

@implementation UserSigninViewController

#pragma mark - Public

- (void)markUnifiedConsentScreenReachedBottom {
  // This is the first time the unified consent screen has reached the bottom.
  if (self.hasUnifiedConsentScreenReachedBottom == NO) {
    self.hasUnifiedConsentScreenReachedBottom = YES;
    [self updatePrimaryButtonStyle];
  }
}

- (void)updatePrimaryButtonStyle {
  if (![self.delegate unifiedConsentCoordinatorHasIdentity]) {
    // User has not added an account. Display 'add account' button.
    [self.confirmationButton setTitle:self.addAccountButtonTitle
                             forState:UIControlStateNormal];
    [self setConfirmationStylingWithButton:self.confirmationButton];
    self.confirmationButton.tag = AuthenticationButtonTypeAddAccount;
  } else if (!self.hasUnifiedConsentScreenReachedBottom) {
    // User has not scrolled to the bottom of the user consent screen.
    // Display 'more' button.
    [self updateButtonAsMoreButton:self.confirmationButton];
    self.confirmationButton.tag = AuthenticationButtonTypeMore;
  } else {
    // By default display 'Yes I'm in' button.
    [self.confirmationButton setTitle:self.confirmationButtonTitle
                             forState:UIControlStateNormal];
    [self setConfirmationStylingWithButton:self.confirmationButton];
    self.confirmationButton.tag = AuthenticationButtonTypeConfirmation;
  }
  [self.confirmationButton addTarget:self
                              action:@selector(onConfirmationButtonPressed:)
                    forControlEvents:UIControlEventTouchUpInside];
}

- (NSUInteger)supportedInterfaceOrientations {
  return IsIPadIdiom() ? [super supportedInterfaceOrientations]
                       : UIInterfaceOrientationMaskPortrait;
}

#pragma mark - MDCActivityIndicator

- (void)startAnimatingActivityIndicator {
  [self addActivityIndicator];
  [self.activityIndicator startAnimating];
}

- (void)stopAnimatingActivityIndicator {
  [self.activityIndicator stopAnimating];
  [self.activityIndicator removeFromSuperview];
  self.activityIndicator = nil;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = self.systemBackgroundColor;

  [self addConfirmationButton];
  [self embedUserConsentView];
  [self addSkipSigninButton];

  [self.view addSubview:self.gradientView];

  // The layout constraints should be added at the end once all of the views
  // have been created.
  AuthenticationViewConstants constants = self.authenticationViewConstants;

  // Embedded view constraints.
  AddSameConstraintsWithInsets(
      self.unifiedConsentViewController.view, self.view.safeAreaLayoutGuide,
      ChromeDirectionalEdgeInsetsMake(0, 0,
                                      constants.ButtonHeight +
                                          constants.ButtonBottomPadding +
                                          constants.ButtonTopPadding,
                                      0));

  // Skip sign-in button constraints.
  AddSameConstraintsToSidesWithInsets(
      self.skipSigninButton, self.view.safeAreaLayoutGuide,
      LayoutSides::kBottom | LayoutSides::kLeading,
      ChromeDirectionalEdgeInsetsMake(0, constants.ButtonHorizontalPadding,
                                      constants.ButtonBottomPadding, 0));

  // Confirmation button constraints.
  AddSameConstraintsToSidesWithInsets(
      self.confirmationButton, self.view.safeAreaLayoutGuide,
      LayoutSides::kBottom | LayoutSides::kTrailing,
      ChromeDirectionalEdgeInsetsMake(0, 0, constants.ButtonBottomPadding,
                                      constants.ButtonHorizontalPadding));

  // Gradient layer constraints.
  self.gradientView.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [self.gradientView.bottomAnchor
        constraintEqualToAnchor:self.unifiedConsentViewController.view
                                    .bottomAnchor],
    [self.gradientView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.gradientView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],

    [self.gradientView.heightAnchor
        constraintEqualToConstant:constants.GradientHeight],
  ]];
}

#pragma mark - Properties

- (UIColor*)systemBackgroundColor {
  return UIColor.cr_systemBackgroundColor;
}

- (NSString*)confirmationButtonTitle {
  return l10n_util::GetNSString(IDS_IOS_ACCOUNT_UNIFIED_CONSENT_OK_BUTTON);
}
- (NSString*)skipSigninButtonTitle {
  if (self.useFirstRunSkipButton) {
    return l10n_util::GetNSString(
        IDS_IOS_FIRSTRUN_ACCOUNT_CONSISTENCY_SKIP_BUTTON);
  }
  return l10n_util::GetNSString(IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON);
}

- (NSString*)addAccountButtonTitle {
  return l10n_util::GetNSString(IDS_IOS_ACCOUNT_UNIFIED_CONSENT_ADD_ACCOUNT);
}

- (NSString*)scrollButtonTitle {
  return l10n_util::GetNSString(
      IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_SCROLL_BUTTON);
}

- (int)acceptSigninButtonStringId {
  return IDS_IOS_ACCOUNT_UNIFIED_CONSENT_OK_BUTTON;
}

- (const AuthenticationViewConstants&)authenticationViewConstants {
  BOOL isRegularSizeClass = IsRegularXRegularSizeClass(self.traitCollection);
  return isRegularSizeClass ? kRegularConstants : kCompactConstants;
}

// Sets up gradient that masks text when the iOS device size is a compact size.
// This is to hint to the user that there is additional text below the end of
// the screen.
- (UIView*)gradientView {
  if (!_gradientView) {
    _gradientView = [[GradientView alloc] init];
  }
  return _gradientView;
}

#pragma mark - Subviews

// Sets up activity indicator properties and adds it to the user sign-in view.
- (void)addActivityIndicator {
  DCHECK(!self.activityIndicator);
  self.activityIndicator =
      [[MDCActivityIndicator alloc] initWithFrame:CGRectZero];
  self.activityIndicator.strokeWidth = 3;
  self.activityIndicator.cycleColors = @[ [UIColor colorNamed:kBlueColor] ];

  [self.view addSubview:self.activityIndicator];

  self.activityIndicator.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameCenterConstraints(self.view, self.activityIndicator);
}

// Embeds the user consent view in the root view.
- (void)embedUserConsentView {
  DCHECK(self.confirmationButton);
  DCHECK(self.unifiedConsentViewController);
  self.unifiedConsentViewController.view
      .translatesAutoresizingMaskIntoConstraints = NO;

  [self addChildViewController:self.unifiedConsentViewController];
  [self.view insertSubview:self.unifiedConsentViewController.view
              belowSubview:self.confirmationButton];
  [self.unifiedConsentViewController didMoveToParentViewController:self];
}

// Sets up confirmation button properties and adds it to the user sign-in view.
- (void)addConfirmationButton {
  DCHECK(self.unifiedConsentViewController);
  self.confirmationButton = [[UIButton alloc] init];
  self.confirmationButton.accessibilityIdentifier = @"ic_close";

  [self addSubviewWithButton:self.confirmationButton];
  // Note that the button style will depend on the user sign-in state.
  [self updatePrimaryButtonStyle];
  self.confirmationButton.contentEdgeInsets =
      UIEdgeInsetsMake(kButtonTitleContentInset, kButtonTitleContentInset,
                       kButtonTitleContentInset, kButtonTitleContentInset);
}

// Sets up skip sign-in button properties and adds it to the user sign-in view.
- (void)addSkipSigninButton {
  DCHECK(!self.skipSigninButton);
  DCHECK(self.unifiedConsentViewController);
  self.skipSigninButton = [[UIButton alloc] init];
  [self addSubviewWithButton:self.skipSigninButton];
  [self.skipSigninButton setTitle:self.skipSigninButtonTitle.uppercaseString
                         forState:UIControlStateNormal];
  [self setSkipSigninStylingWithButton:self.skipSigninButton];
  [self.skipSigninButton addTarget:self
                            action:@selector(onSkipSigninButtonPressed:)
                  forControlEvents:UIControlEventTouchUpInside];
  self.skipSigninButton.contentEdgeInsets =
      UIEdgeInsetsMake(kButtonTitleContentInset, kButtonTitleContentInset,
                       kButtonTitleContentInset, kButtonTitleContentInset);
}

// Sets up button properties and adds it to view.
- (void)addSubviewWithButton:(UIButton*)button {
  button.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  [self.view addSubview:button];
  button.translatesAutoresizingMaskIntoConstraints = NO;
}

#pragma mark - Styling

- (void)setConfirmationStylingWithButton:(UIButton*)button {
  DCHECK(button);
  button.backgroundColor = [UIColor colorNamed:kBlueColor];
  button.layer.cornerRadius = kButtonCornerRadius;
  [button setTitleColor:[UIColor colorNamed:kSolidButtonTextColor]
               forState:UIControlStateNormal];
  [button setImage:nil forState:UIControlStateNormal];
}

- (void)setSkipSigninStylingWithButton:(UIButton*)button {
  DCHECK(button);
  button.backgroundColor = self.systemBackgroundColor;
  [button setTitleColor:[UIColor colorNamed:kBlueColor]
               forState:UIControlStateNormal];
}

- (void)updateButtonAsMoreButton:(UIButton*)button {
  [button setTitle:self.scrollButtonTitle forState:UIControlStateNormal];
  UIImage* buttonImage = [[UIImage imageNamed:@"signin_confirmation_more"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  [button setImage:buttonImage forState:UIControlStateNormal];
  [self setSkipSigninStylingWithButton:button];
}

#pragma mark - Events

- (void)onSkipSigninButtonPressed:(id)sender {
  DCHECK_EQ(self.skipSigninButton, sender);

  [self stopAnimatingActivityIndicator];
  [self.delegate userSigninViewControllerDidTapOnSkipSignin];
}

- (void)onConfirmationButtonPressed:(id)sender {
  DCHECK_EQ(self.confirmationButton, sender);

  switch (self.confirmationButton.tag) {
    case AuthenticationButtonTypeMore: {
      [self.delegate userSigninViewControllerDidScrollOnUnifiedConsent];
      break;
    }
    case AuthenticationButtonTypeAddAccount: {
      [self.delegate userSigninViewControllerDidTapOnAddAccount];
      break;
    }
    case AuthenticationButtonTypeConfirmation: {
      [self startAnimatingActivityIndicator];
      [self.delegate userSigninViewControllerDidTapOnSignin];
      break;
    }
  }
}

@end
