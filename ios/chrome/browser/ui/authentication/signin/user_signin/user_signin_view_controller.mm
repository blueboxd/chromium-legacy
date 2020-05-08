// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_signin_view_controller.h"

#include "base/feature_list.h"
#import "base/logging.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/gradient_view.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
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
// Gradient height.
const CGFloat kGradientHeight = 40.;
// Max size for the user consent view.
const CGFloat kUserConsentMaxSize = 600.;
// Image inset for the more button.
const CGFloat kImageInset = 16.0;

// Layout constants for buttons.
struct AuthenticationViewConstants {
  CGFloat PrimaryFontSize;
  CGFloat SecondaryFontSize;
  CGFloat ButtonHeight;
  CGFloat ButtonHorizontalPadding;
  CGFloat ButtonVerticalPadding;
  CGFloat ButtonTitleContentHorizontalInset;
  CGFloat ButtonTitleContentVerticalInset;
};

const AuthenticationViewConstants kCompactConstants = {
    24,  // PrimaryFontSize
    14,  // SecondaryFontSize
    36,  // ButtonHeight
    16,  // ButtonHorizontalPadding
    16,  // ButtonVerticalPadding
    16,  // ButtonTitleContentHorizontalInset
    8,   // ButtonTitleContentVerticalInset
};

const AuthenticationViewConstants kRegularConstants = {
    1.5 * kCompactConstants.PrimaryFontSize,
    1.5 * kCompactConstants.SecondaryFontSize,
    1.5 * kCompactConstants.ButtonHeight,
    32,  // ButtonHorizontalPadding
    32,  // ButtonVerticalPadding
    16,  // ButtonTitleContentInset
    16,  // ButtonTitleContentInset
};

// The style applied to a button type.
enum AuthenticationButtonType {
  AuthenticationButtonTypeMore,
  AuthenticationButtonTypeAddAccount,
  AuthenticationButtonTypeConfirmation,
};
}  // namespace

@interface UserSigninViewController ()

// Container view used to center vertically the user consent view between the
// top of the controller view and the top of the button view.
@property(nonatomic, strong) UIView* containerView;
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
@property(nonatomic, strong, readonly) GradientView* gradientView;
// Lists of constraints that need to be activated when the view is in
// compact size class.
@property(nonatomic, strong, readonly) NSArray* compactSizeClassConstraints;
// Lists of constraints that need to be activated when the view is in
// regular size class.
@property(nonatomic, strong, readonly) NSArray* regularSizeClassConstraints;

@end

@implementation UserSigninViewController

@synthesize gradientView = _gradientView;
@synthesize compactSizeClassConstraints = _compactSizeClassConstraints;
@synthesize regularSizeClassConstraints = _regularSizeClassConstraints;

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

- (void)signinWillStart {
  self.confirmationButton.hidden = YES;
  [self startAnimatingActivityIndicator];
}

- (void)signinDidStop {
  self.confirmationButton.hidden = NO;
  [self stopAnimatingActivityIndicator];
}

#pragma mark - AuthenticationFlowDelegate

- (void)didPresentDialog {
  [self.activityIndicator stopAnimating];
}

- (void)didDismissDialog {
  [self.activityIndicator startAnimating];
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
  [self addSkipSigninButton];
  [self embedUserConsentView];

  [NSLayoutConstraint activateConstraints:@[
    // Contraints for the container view. The bottom constraint has to be set
    // according to the size class.
    [self.containerView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [self.containerView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.containerView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
  ]];

  [self.containerView addSubview:self.gradientView];
  self.gradientView.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    // The gradient view needs to be attatched to the bottom of the user
    // consent view which contains the scroll view.
    [self.gradientView.bottomAnchor
        constraintEqualToAnchor:self.unifiedConsentViewController.view
                                    .bottomAnchor],
    [self.gradientView.leadingAnchor
        constraintEqualToAnchor:self.unifiedConsentViewController.view
                                    .leadingAnchor],
    [self.gradientView.trailingAnchor
        constraintEqualToAnchor:self.unifiedConsentViewController.view
                                    .trailingAnchor],
    [self.gradientView.heightAnchor constraintEqualToConstant:kGradientHeight],
  ]];

  [self updateViewsAndConstraints];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  [self updateViewsAndConstraints];
}

#pragma mark - Constraints

// Generate default constraints based on the constants.
- (NSMutableArray*)generateConstraintsWithConstants:
    (AuthenticationViewConstants)constants {
  NSMutableArray* constraints = [NSMutableArray array];
  // Confirmation button constraints
  [constraints addObjectsFromArray:@[
    [self.view.safeAreaLayoutGuide.trailingAnchor
        constraintEqualToAnchor:self.confirmationButton.trailingAnchor
                       constant:constants.ButtonHorizontalPadding],
    [self.view.safeAreaLayoutGuide.bottomAnchor
        constraintEqualToAnchor:self.confirmationButton.bottomAnchor
                       constant:constants.ButtonVerticalPadding],
  ]];
  // Skip button constraints
  [constraints addObjectsFromArray:@[
    [self.skipSigninButton.leadingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor
                       constant:constants.ButtonHorizontalPadding],
    [self.view.safeAreaLayoutGuide.bottomAnchor
        constraintEqualToAnchor:self.skipSigninButton.bottomAnchor
                       constant:constants.ButtonVerticalPadding],
  ]];
  return constraints;
}

// Updates view sizes and constraints.
- (void)updateViewsAndConstraints {
  BOOL isRegularSizeClass = IsRegularXRegularSizeClass(self.traitCollection);
  UIFontTextStyle fontStyle;
  if (isRegularSizeClass) {
    [NSLayoutConstraint deactivateConstraints:self.compactSizeClassConstraints];
    [NSLayoutConstraint activateConstraints:self.regularSizeClassConstraints];
    fontStyle = UIFontTextStyleTitle2;
  } else {
    [NSLayoutConstraint deactivateConstraints:self.regularSizeClassConstraints];
    [NSLayoutConstraint activateConstraints:self.compactSizeClassConstraints];
    fontStyle = UIFontTextStyleSubheadline;
  }
  [self applyDefaultSizeWithButton:self.confirmationButton fontStyle:fontStyle];
  [self applyDefaultSizeWithButton:self.skipSigninButton fontStyle:fontStyle];
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

- (UIView*)gradientView {
  if (!_gradientView) {
    _gradientView = [[GradientView alloc] init];
  }
  return _gradientView;
}

- (NSArray*)compactSizeClassConstraints {
  if (!_compactSizeClassConstraints) {
    NSMutableArray* constraints =
        [self generateConstraintsWithConstants:kCompactConstants];
    [constraints addObjectsFromArray:@[
      // Constraints for the user consent view inside the container view.
      [self.unifiedConsentViewController.view.topAnchor
          constraintEqualToAnchor:self.containerView.topAnchor],
      [self.unifiedConsentViewController.view.bottomAnchor
          constraintEqualToAnchor:self.containerView.bottomAnchor],
      [self.unifiedConsentViewController.view.leadingAnchor
          constraintEqualToAnchor:self.containerView.leadingAnchor],
      [self.unifiedConsentViewController.view.trailingAnchor
          constraintEqualToAnchor:self.containerView.trailingAnchor],
      // Constraint between the container view and the confirmation button.
      [self.confirmationButton.topAnchor
          constraintEqualToAnchor:self.containerView.bottomAnchor
                         constant:kCompactConstants.ButtonVerticalPadding],
    ]];
    _compactSizeClassConstraints = constraints;
  }
  return _compactSizeClassConstraints;
}

- (NSArray*)regularSizeClassConstraints {
  if (!_regularSizeClassConstraints) {
    NSMutableArray* constraints =
        [self generateConstraintsWithConstants:kRegularConstants];
    [constraints addObjectsFromArray:@[
      // Constraints for the user consent view inside the container view, to
      // make sure it is never bigger than the container view.
      [self.unifiedConsentViewController.view.topAnchor
          constraintGreaterThanOrEqualToAnchor:self.containerView.topAnchor],
      [self.unifiedConsentViewController.view.bottomAnchor
          constraintLessThanOrEqualToAnchor:self.containerView.bottomAnchor],
      [self.unifiedConsentViewController.view.leadingAnchor
          constraintGreaterThanOrEqualToAnchor:self.containerView
                                                   .leadingAnchor],
      [self.unifiedConsentViewController.view.trailingAnchor
          constraintLessThanOrEqualToAnchor:self.containerView.trailingAnchor],
      // The user consent view needs to be centered if the container view is
      // bigger than the max size authorized for the user consent view.
      [self.unifiedConsentViewController.view.centerXAnchor
          constraintEqualToAnchor:self.containerView.centerXAnchor],
      [self.unifiedConsentViewController.view.centerYAnchor
          constraintEqualToAnchor:self.containerView.centerYAnchor],
      // Constraint between the container view and the confirmation button.
      [self.confirmationButton.topAnchor
          constraintEqualToAnchor:self.containerView.bottomAnchor
                         constant:kRegularConstants.ButtonVerticalPadding],
    ]];
    // Adding constraints to ensure the user consent view has a limited size
    // on iPad. If the screen is bigger than the max size, those constraints
    // limit the user consent view.
    // If the screen is smaller than the max size, those constraints are ignored
    // since they have a lower priority than the constraints set aboved.
    NSArray* lowerPriorityConstraints = @[
      [self.unifiedConsentViewController.view.heightAnchor
          constraintEqualToConstant:kUserConsentMaxSize],
      [self.unifiedConsentViewController.view.widthAnchor
          constraintEqualToConstant:kUserConsentMaxSize],
    ];
    for (NSLayoutConstraint* constraints in lowerPriorityConstraints) {
      // We do not use |UILayoutPriorityDefaultHigh| because it makes some
      // multiline labels on one line and truncated on iPad.
      constraints.priority = UILayoutPriorityRequired - 1;
    }
    [constraints addObjectsFromArray:lowerPriorityConstraints];
    _regularSizeClassConstraints = constraints;
  }
  return _regularSizeClassConstraints;
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
  AddSameCenterConstraints(self.confirmationButton, self.activityIndicator);
}

// Embeds the user consent view in the root view.
- (void)embedUserConsentView {
  DCHECK(self.confirmationButton);
  DCHECK(self.unifiedConsentViewController);
  self.unifiedConsentViewController.view
      .translatesAutoresizingMaskIntoConstraints = NO;

  self.containerView = [[UIView alloc] init];
  self.containerView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:self.containerView];
  [self addChildViewController:self.unifiedConsentViewController];
  [self.containerView addSubview:self.unifiedConsentViewController.view];
  [self.unifiedConsentViewController didMoveToParentViewController:self];
}

// Sets up confirmation button properties and adds it to the user sign-in view.
- (void)addConfirmationButton {
  DCHECK(self.unifiedConsentViewController);
  self.confirmationButton = [[UIButton alloc] init];
#if defined(__IPHONE_13_4)
  if (@available(iOS 13.4, *)) {
    if (base::FeatureList::IsEnabled(kPointerSupport)) {
      self.confirmationButton.pointerInteractionEnabled = YES;
    }
  }
#endif  // defined(__IPHONE_13_4)

  [self addSubviewWithButton:self.confirmationButton];
  // Note that the button style will depend on the user sign-in state.
  [self updatePrimaryButtonStyle];
}

// Sets up skip sign-in button properties and adds it to the user sign-in view.
- (void)addSkipSigninButton {
  DCHECK(!self.skipSigninButton);
  DCHECK(self.unifiedConsentViewController);
  self.skipSigninButton = [[UIButton alloc] init];
  self.skipSigninButton.accessibilityIdentifier =
      kSkipSigninAccessibilityIdentifier;
  [self addSubviewWithButton:self.skipSigninButton];
  [self.skipSigninButton setTitle:self.skipSigninButtonTitle
                         forState:UIControlStateNormal];
  [self setSkipSigninStylingWithButton:self.skipSigninButton];
  [self.skipSigninButton addTarget:self
                            action:@selector(onSkipSigninButtonPressed:)
                  forControlEvents:UIControlEventTouchUpInside];
#if defined(__IPHONE_13_4)
  if (@available(iOS 13.4, *)) {
    if (base::FeatureList::IsEnabled(kPointerSupport)) {
      self.skipSigninButton.pointerInteractionEnabled = YES;
    }
  }
#endif  // defined(__IPHONE_13_4)
}

// Sets up button properties and adds it to view.
- (void)addSubviewWithButton:(UIButton*)button {
  [button
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisVertical];
  [button setContentHuggingPriority:UILayoutPriorityRequired
                            forAxis:UILayoutConstraintAxisVertical];
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
  button.imageEdgeInsets = UIEdgeInsetsMake(0, -kImageInset, 0, 0);
}

// Applies font and inset to |button| according to the current size class.
- (void)applyDefaultSizeWithButton:(UIButton*)button
                         fontStyle:(UIFontTextStyle)fontStyle {
  const AuthenticationViewConstants& constants =
      self.authenticationViewConstants;
  CGFloat horizontalContentInset = constants.ButtonTitleContentHorizontalInset;
  CGFloat verticalContentInset = constants.ButtonTitleContentVerticalInset;
  button.contentEdgeInsets =
      UIEdgeInsetsMake(verticalContentInset, horizontalContentInset,
                       verticalContentInset, horizontalContentInset);
  button.titleLabel.font = [UIFont preferredFontForTextStyle:fontStyle];
}

#pragma mark - Events

- (void)onSkipSigninButtonPressed:(id)sender {
  DCHECK_EQ(self.skipSigninButton, sender);
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
      [self.delegate userSigninViewControllerDidTapOnSignin];
      break;
    }
  }
}

@end
