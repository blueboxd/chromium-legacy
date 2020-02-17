// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_signin_view_controller.h"

#import "base/logging.h"
#import "ios/chrome/browser/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/ActivityIndicator/src/MaterialActivityIndicator.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Inset between button contents and edge.
const CGFloat kButtonTitleContentInset = 8.0;

// Layout constants for buttons.
struct AuthenticationViewConstants {
  CGFloat ButtonHeight;
  CGFloat ButtonHorizontalPadding;
  CGFloat ButtonTopPadding;
  CGFloat ButtonBottomPadding;
};

const AuthenticationViewConstants kCompactConstants = {
    36,  // ButtonHeight
    16,  // ButtonHorizontalPadding
    16,  // ButtonTopPadding
    16,  // ButtonBottomPadding
};

const AuthenticationViewConstants kRegularConstants = {
    1.5 * kCompactConstants.ButtonHeight,
    32,  // ButtonHorizontalPadding
    32,  // ButtonTopPadding
    32,  // ButtonBottomPadding
};
}

@interface UserSigninViewController ()

// Activity indicator used to block the UI when a sign-in operation is in
// progress.
@property(nonatomic, strong) MDCActivityIndicator* activityIndicator;
// Button used to confirm the sign-in operation, e.g. "Yes I'm In".
@property(nonatomic, strong) UIButton* confirmationButton;
// Button used to exit the sign-in operation without confirmation, e.g. "No
// Thanks", "Cancel".
@property(nonatomic, strong) UIButton* skipSigninButton;

@end

@implementation UserSigninViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = self.systemBackgroundColor;

  [self addConfirmationButtonToView];
  [self embedUserConsentView];
  [self addActivityIndicatorToView];
  [self addSkipSigninButtonToView];

  // The layout constraints should be added at the end once all of the views
  // have been created.
  AuthenticationViewConstants constants = self.authenticationViewConstants;
  AddSameConstraintsWithInsets(
      self.unifiedConsentViewController.view, self.view,
      ChromeDirectionalEdgeInsetsMake(0, 0,
                                      constants.ButtonHeight +
                                          constants.ButtonBottomPadding +
                                          constants.ButtonTopPadding,
                                      0));
  AddSameCenterConstraints(self.view, self.activityIndicator);
  AddSameConstraintsToSidesWithInsets(
      self.skipSigninButton, self.view,
      LayoutSides::kBottom | LayoutSides::kLeading,
      ChromeDirectionalEdgeInsetsMake(0, constants.ButtonHorizontalPadding,
                                      constants.ButtonBottomPadding, 0));
  AddSameConstraintsToSidesWithInsets(
      self.confirmationButton, self.view,
      LayoutSides::kBottom | LayoutSides::kTrailing,
      ChromeDirectionalEdgeInsetsMake(0, 0, constants.ButtonBottomPadding,
                                      constants.ButtonHorizontalPadding));
}

#pragma mark - Properties

- (UIColor*)systemBackgroundColor {
  return UIColor.cr_systemBackgroundColor;
}

- (NSString*)confirmationButtonTitle {
  return l10n_util::GetNSString(IDS_IOS_ACCOUNT_UNIFIED_CONSENT_OK_BUTTON);
}
- (NSString*)skipSigninButtonTitle {
  return l10n_util::GetNSString(IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON);
}

- (const AuthenticationViewConstants&)authenticationViewConstants {
  BOOL isRegularSizeClass = IsRegularXRegularSizeClass(self.traitCollection);
  return isRegularSizeClass ? kRegularConstants : kCompactConstants;
}

#pragma mark - Subviews

// Sets up activity indicator properties and adds it to the view.
- (void)addActivityIndicatorToView {
  DCHECK(!self.activityIndicator);
  self.activityIndicator =
      [[MDCActivityIndicator alloc] initWithFrame:CGRectZero];
  self.activityIndicator.strokeWidth = 3;
  self.activityIndicator.cycleColors = @[ [UIColor colorNamed:kBlueColor] ];
  self.activityIndicator.translatesAutoresizingMaskIntoConstraints = NO;

  [self.view addSubview:self.activityIndicator];
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

// Sets up confirmation button properties and adds it to the view.
- (void)addConfirmationButtonToView {
  DCHECK(self.unifiedConsentViewController);
  self.confirmationButton = [[UIButton alloc] init];
  self.confirmationButton.accessibilityIdentifier = @"ic_close";

  [self addSubviewWithButton:self.confirmationButton
                       title:self.confirmationButtonTitle];
  [self setConfirmationStylingWithButton:self.confirmationButton];

  self.confirmationButton.contentEdgeInsets =
      UIEdgeInsetsMake(kButtonTitleContentInset, kButtonTitleContentInset,
                       kButtonTitleContentInset, kButtonTitleContentInset);
}

// Sets up skip sign-in button properties and adds it to the view.
- (void)addSkipSigninButtonToView {
  DCHECK(!self.skipSigninButton);
  DCHECK(self.unifiedConsentViewController);
  self.skipSigninButton = [[UIButton alloc] init];
  [self addSubviewWithButton:self.skipSigninButton
                       title:self.skipSigninButtonTitle];
  [self setSkipSigninStylingWithButton:self.skipSigninButton];
}

// Sets up button properties and adds it to view.
- (void)addSubviewWithButton:(UIButton*)button title:(NSString*)title {
  [button setTitle:title forState:UIControlStateNormal];
  button.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];

  [self.view addSubview:button];
  [button addTarget:self
                action:@selector(onButtonPressed:)
      forControlEvents:UIControlEventTouchUpInside];

  button.translatesAutoresizingMaskIntoConstraints = NO;
}

#pragma mark - Styling

- (void)setConfirmationStylingWithButton:(UIButton*)button {
  DCHECK(button);
  button.backgroundColor = [UIColor colorNamed:kBlueColor];
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

#pragma mark - Events

- (void)onButtonPressed:(id)sender {
  // TODO(crbug.com/971989): Populate action.
  NOTIMPLEMENTED();
}

@end
