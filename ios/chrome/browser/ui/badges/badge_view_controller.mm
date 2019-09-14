// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/badges/badge_view_controller.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/badges/badge_button.h"
#import "ios/chrome/browser/ui/badges/badge_button_factory.h"
#import "ios/chrome/browser/ui/badges/badge_item.h"
#import "ios/chrome/browser/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// FullScreen progress threshold in which to toggle between full screen on and
// off mode for the badge view.
const double kFullScreenProgressThreshold = 0.85;

}  // namespace

@interface BadgeViewController ()

// Button factory.
@property(nonatomic, strong) BadgeButtonFactory* buttonFactory;

// BadgeButton to show when in FullScreen (i.e. when the
// toolbars are expanded). Setting this property will add the button to the
// StackView.
@property(nonatomic, strong) BadgeButton* displayedBadge;

// BadgeButton to show in both FullScreen and non FullScreen.
@property(nonatomic, strong) BadgeButton* fullScreenBadge;

// Array of all available badges.
@property(nonatomic, strong) NSMutableArray<BadgeButton*>* badges;

// StackView holding the displayedBadge and fullScreenBadge.
@property(nonatomic, strong) UIStackView* stackView;

@end

@implementation BadgeViewController

- (instancetype)initWithButtonFactory:(BadgeButtonFactory*)buttonFactory {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    DCHECK(buttonFactory);
    _buttonFactory = buttonFactory;
    _stackView = [[UIStackView alloc] init];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.stackView.translatesAutoresizingMaskIntoConstraints = NO;
  self.stackView.axis = UILayoutConstraintAxisHorizontal;
  [self.view addSubview:self.stackView];
  AddSameConstraints(self.view, self.stackView);
}

#pragma mark - Protocols

#pragma mark BadgeConsumer

- (void)setupWithDisplayedBadge:(id<BadgeItem>)displayedBadgeItem
                fullScreenBadge:(id<BadgeItem>)fullscreenBadgeItem {
  self.displayedBadge = nil;
  self.fullScreenBadge = nil;
  if (displayedBadgeItem) {
    BadgeButton* newButton = [self.buttonFactory
        getBadgeButtonForBadgeType:displayedBadgeItem.badgeType];
    [newButton setAccepted:displayedBadgeItem.accepted animated:NO];
    self.displayedBadge = newButton;
  }
  if (fullscreenBadgeItem) {
    self.fullScreenBadge = [self.buttonFactory
        getBadgeButtonForBadgeType:fullscreenBadgeItem.badgeType];
  }
}

- (void)updateDisplayedBadge:(id<BadgeItem>)displayedBadgeItem
             fullScreenBadge:(id<BadgeItem>)fullscreenBadgeItem {
  if (fullscreenBadgeItem) {
    if (!self.fullScreenBadge ||
        self.fullScreenBadge.badgeType != fullscreenBadgeItem.badgeType) {
      BadgeButton* newButton = [self.buttonFactory
          getBadgeButtonForBadgeType:fullscreenBadgeItem.badgeType];
      self.fullScreenBadge = newButton;
    }
  } else {
    self.fullScreenBadge = nil;
  }

  if (displayedBadgeItem) {
    if (self.displayedBadge &&
        self.displayedBadge.badgeType == displayedBadgeItem.badgeType) {
      [self.displayedBadge setAccepted:displayedBadgeItem.accepted
                              animated:YES];
    } else {
      BadgeButton* newButton = [self.buttonFactory
          getBadgeButtonForBadgeType:displayedBadgeItem.badgeType];
      [newButton setAccepted:displayedBadgeItem.accepted animated:NO];
      self.displayedBadge = newButton;
    }
  } else {
    self.displayedBadge = nil;
  }
}

#pragma mark FullscreenUIElement

- (void)updateForFullscreenProgress:(CGFloat)progress {
  BOOL badgeViewShouldCollapse = progress <= kFullScreenProgressThreshold;
  if (badgeViewShouldCollapse) {
    self.fullScreenBadge.fullScreenOn = YES;
    // Fade in/out in the FullScreen badge with the FullScreen on
    // configurations.
    CGFloat alphaValue = fmax((kFullScreenProgressThreshold - progress) /
                                  kFullScreenProgressThreshold,
                              0);
    self.fullScreenBadge.alpha = alphaValue;
  } else {
    self.fullScreenBadge.fullScreenOn = NO;
    // Fade in/out the FullScreen badge with the FullScreen off configurations
    // at a speed matching that of the trailing button in the
    // LocationBarSteadyView.
    CGFloat alphaValue = fmax((progress - kFullScreenProgressThreshold) /
                                  (1 - kFullScreenProgressThreshold),
                              0);
    self.displayedBadge.alpha = alphaValue;
    self.fullScreenBadge.alpha = alphaValue;
  }
}

#pragma mark - Getter/Setter

- (void)setDisplayedBadge:(BadgeButton*)badgeButton {
  [self.stackView removeArrangedSubview:_displayedBadge];
  [_displayedBadge removeFromSuperview];
  if (!badgeButton) {
    _displayedBadge = nil;
    return;
  }
  _displayedBadge = badgeButton;
  [self.stackView addArrangedSubview:_displayedBadge];
}

- (void)setFullScreenBadge:(BadgeButton*)fullScreenBadge {
  [self.stackView removeArrangedSubview:_fullScreenBadge];
  [_fullScreenBadge removeFromSuperview];
  if (!fullScreenBadge) {
    _fullScreenBadge = nil;
    return;
  }
  _fullScreenBadge = fullScreenBadge;
  [self.stackView insertArrangedSubview:_fullScreenBadge atIndex:0];
}

#pragma mark - Helpers

- (void)updateDisplayedBadges {
  self.displayedBadge = [self.badges lastObject];
}

@end
