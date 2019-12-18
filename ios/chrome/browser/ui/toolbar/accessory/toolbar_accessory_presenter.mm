// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/accessory/toolbar_accessory_presenter.h"

#include "base/i18n/rtl.h"
#import "base/logging.h"
#import "ios/chrome/browser/ui/image_util/image_util.h"
#import "ios/chrome/browser/ui/toolbar/accessory/toolbar_accessory_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/browser/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/dynamic_color_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kToolbarAccessoryWidthRegularRegular = 375;
const CGFloat kToolbarAccessoryCornerRadiusRegularRegular = 13;
const CGFloat kRegularRegularHorizontalMargin = 5;

// Margin between the beginning of the shadow image and the content being
// shadowed.
const CGFloat kShadowMargin = 196;

// Toolbar Accessory animation drop down duration.
const CGFloat kAnimationDuration = 0.15;
}  // namespace

@interface ToolbarAccessoryPresenter ()

// The view controller to present views into.
@property(nonatomic, weak, readonly) UIViewController* baseViewController;

// The view that acts as the background for |presentedView|, redefined as
// readonly. This is especially important on iPhone, as this view that holds
// everything around the safe area.
@property(nonatomic, strong, readwrite) UIView* backgroundView;
// The input view being presented, stored as a weak reference.
@property(nonatomic, weak) UIView* presentedView;

@property(nonatomic, assign) BOOL isIncognito;

@end

@implementation ToolbarAccessoryPresenter

#pragma mark - Public

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                               isIncognito:(BOOL)isIncognito {
  if (self = [super init]) {
    _baseViewController = baseViewController;
    _isIncognito = isIncognito;
  }
  return self;
}

- (void)addToolbarAccessoryView:(UIView*)toolbarAccessoryView
                       animated:(BOOL)animated
                     completion:(void (^)())completion {
  if (self.backgroundView) {
    DCHECK(toolbarAccessoryView == self.presentedView);
    return;
  }

  if (ShouldShowCompactToolbar()) {
    [self showIPhoneToolbarAccessoryView:toolbarAccessoryView
                                animated:animated
                              completion:completion];
  } else {
    [self showIPadToolbarAccessoryView:toolbarAccessoryView
                              animated:animated
                            completion:completion];
  }
}

- (void)hideToolbarAccessoryViewAnimated:(BOOL)animated
                              completion:(void (^)())completion {
  // If view is nil, nothing to hide.
  if (!self.backgroundView) {
    return;
  }

  if (animated) {
    void (^animation)();
    if (ShouldShowCompactToolbar()) {
      CGRect oldFrame = self.backgroundView.frame;
      self.backgroundView.layer.anchorPoint = CGPointMake(0.5, 0);
      self.backgroundView.frame = oldFrame;
      animation = ^{
        self.backgroundView.transform = CGAffineTransformMakeScale(1, 0.05);
        self.backgroundView.alpha = 0;
      };
    } else {
      CGFloat rtlModifier = base::i18n::IsRTL() ? -1 : 1;
      animation = ^{
        self.backgroundView.transform = CGAffineTransformMakeTranslation(
            rtlModifier * self.backgroundView.bounds.size.width, 0);
      };
    }
    [UIView animateWithDuration:kAnimationDuration
                     animations:animation
                     completion:^(BOOL finished) {
                       [self teardownView];
                       if (completion) {
                         completion();
                       }
                     }];
  } else {
    [self teardownView];
    if (completion) {
      completion();
    }
  }
}

#pragma mark - Private Helpers

// Animates accessory to iPad positioning: A small view in the top right, just
// below the toolbar.
- (void)showIPadToolbarAccessoryView:(UIView*)toolbarAccessoryView
                            animated:(BOOL)animated
                          completion:(void (^)())completion {
  DCHECK(IsIPadIdiom());
  self.backgroundView =
      [self createBackgroundViewForToolbarAccessoryView:toolbarAccessoryView];
  self.backgroundView.layer.cornerRadius =
      kToolbarAccessoryCornerRadiusRegularRegular;
  [self.baseViewController.view addSubview:self.backgroundView];

  UIImageView* shadow =
      [[UIImageView alloc] initWithImage:StretchableImageNamed(@"menu_shadow")];
  shadow.translatesAutoresizingMaskIntoConstraints = NO;
  [self.backgroundView addSubview:shadow];

  // The width should be this value, unless that is too wide for the screen.
  // Using a less than required priority means that the view will attempt to be
  // this width, but use the less than or equal constraint below if the view
  // is too narrow.
  NSLayoutConstraint* widthConstraint = [self.backgroundView.widthAnchor
      constraintEqualToConstant:kToolbarAccessoryWidthRegularRegular];
  widthConstraint.priority = UILayoutPriorityRequired - 1;

  // This constraint sets the view in the pre-animation state. It will be
  // removed and replaced during the animation process.
  NSLayoutConstraint* animationConstraint = [self.backgroundView.leadingAnchor
      constraintEqualToAnchor:self.baseViewController.view.trailingAnchor];

  UILayoutGuide* toolbarLayoutGuide =
      [NamedGuide guideWithName:kPrimaryToolbarGuide
                           view:self.baseViewController.view];

  [NSLayoutConstraint activateConstraints:@[
    // Anchors accessory below the |toolbarView|.
    [self.backgroundView.topAnchor
        constraintEqualToAnchor:toolbarLayoutGuide.bottomAnchor],
    animationConstraint,
    widthConstraint,
    [self.backgroundView.widthAnchor
        constraintLessThanOrEqualToAnchor:self.baseViewController.view
                                              .widthAnchor
                                 constant:-2 * kRegularRegularHorizontalMargin],
    [self.backgroundView.heightAnchor
        constraintEqualToConstant:kAdaptiveToolbarHeight],
  ]];
  // Layouts |shadow| around |self.backgroundView|.
  AddSameConstraintsToSidesWithInsets(
      shadow, self.backgroundView,
      LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kBottom |
          LayoutSides::kTrailing,
      {-kShadowMargin, -kShadowMargin, -kShadowMargin, -kShadowMargin});

  // Force everything to layout in the pre-animation state.
  [self.baseViewController.view layoutIfNeeded];

  // Set up the post-animation positioning constraints.
  animationConstraint.active = NO;
  [self.backgroundView.trailingAnchor
      constraintEqualToAnchor:self.baseViewController.view.trailingAnchor
                     constant:-kRegularRegularHorizontalMargin]
      .active = YES;

  CGFloat duration = animated ? kAnimationDuration : 0;
  [UIView animateWithDuration:duration
      animations:^() {
        [self.backgroundView layoutIfNeeded];
      }
      completion:^(BOOL finished) {
        if (completion) {
          completion();
        }
      }];
}

// Animates accessory to iPhone positioning: covering the whole toolbar.
- (void)showIPhoneToolbarAccessoryView:(UIView*)toolbarAccessoryView
                              animated:(BOOL)animated
                            completion:(nullable void (^)())completion {
  self.backgroundView =
      [self createBackgroundViewForToolbarAccessoryView:toolbarAccessoryView];
  [self.baseViewController.view addSubview:self.backgroundView];

  // This constraint sets up the view to where it should be before the animation
  // starts. It will later be replaced with another constraint for its final
  // position.
  NSLayoutConstraint* animationConstraint = [self.backgroundView.bottomAnchor
      constraintEqualToAnchor:self.baseViewController.view.topAnchor];

  [NSLayoutConstraint activateConstraints:@[
    [self.backgroundView.leadingAnchor
        constraintEqualToAnchor:self.baseViewController.view.leadingAnchor],
    [self.backgroundView.trailingAnchor
        constraintEqualToAnchor:self.baseViewController.view.trailingAnchor],
    [self.presentedView.topAnchor
        constraintGreaterThanOrEqualToAnchor:self.backgroundView
                                                 .safeAreaLayoutGuide
                                                 .topAnchor],
    animationConstraint,
  ]];

  // Force initial layout before the animation.
  [self.baseViewController.view layoutIfNeeded];

  animationConstraint.active = NO;
  [self.backgroundView.topAnchor
      constraintEqualToAnchor:self.baseViewController.view.topAnchor]
      .active = YES;

  CGFloat duration = animated ? kAnimationDuration : 0;
  [UIView animateWithDuration:duration
      animations:^() {
        [self.backgroundView layoutIfNeeded];
      }
      completion:^(BOOL finished) {
        if (completion) {
          completion();
        }
      }];
}

// Creates the background view and adds the |toolbarAccessoryView| to it.
- (UIView*)createBackgroundViewForToolbarAccessoryView:
    (UIView*)toolbarAccessoryView {
  UIView* backgroundView = [[UIView alloc] init];
  backgroundView.translatesAutoresizingMaskIntoConstraints = NO;
  backgroundView.accessibilityIdentifier = kToolbarAccessoryContainerViewID;
  if (@available(iOS 13, *)) {
    // When iOS 12 is dropped, only the next line is needed for styling.
    // Every other check for |incognitoStyle| can be removed, as well as
    // the incognito specific assets.
    backgroundView.overrideUserInterfaceStyle =
        self.isIncognito ? UIUserInterfaceStyleDark
                         : UIUserInterfaceStyleUnspecified;
  }
  backgroundView.backgroundColor = color::DarkModeDynamicColor(
      [UIColor colorNamed:kBackgroundColor], self.isIncognito,
      [UIColor colorNamed:kBackgroundDarkColor]);
  self.presentedView = toolbarAccessoryView;

  [backgroundView addSubview:self.presentedView];

  [NSLayoutConstraint activateConstraints:@[
    [self.presentedView.trailingAnchor
        constraintEqualToAnchor:backgroundView.trailingAnchor],
    [self.presentedView.leadingAnchor
        constraintEqualToAnchor:backgroundView.leadingAnchor],
    [self.presentedView.heightAnchor
        constraintEqualToConstant:kAdaptiveToolbarHeight],
    [self.presentedView.bottomAnchor
        constraintEqualToAnchor:backgroundView.bottomAnchor],
  ]];

  return backgroundView;
}

// Removes the background view from the view hierarchy and performs any
// necessary cleanup.
- (void)teardownView {
  [self.backgroundView removeFromSuperview];
  self.backgroundView = nil;
}

@end
