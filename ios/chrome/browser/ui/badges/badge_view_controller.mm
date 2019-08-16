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

- (void)setupWithBadges:(NSArray*)badges {
  if (!self.badges) {
    self.badges = [[NSMutableArray alloc] init];
  }
  self.fullScreenBadge = nil;
  self.displayedBadge = nil;
  [self.badges removeAllObjects];
  for (id<BadgeItem> item in badges) {
    BadgeButton* newButton =
        [self.buttonFactory getBadgeButtonForBadgeType:item.badgeType];
    // No need to animate this change since it is the initial state.
    [newButton setAccepted:item.accepted animated:NO];
    [self.badges addObject:newButton];
  }
  [self updateDisplayedBadges];
}

- (void)addBadge:(id<BadgeItem>)badgeItem {
  BadgeButton* newButton =
      [self.buttonFactory getBadgeButtonForBadgeType:badgeItem.badgeType];
  // The incognito badge is one that must be visible at all times and persist
  // when fullscreen is expanded and collapsed.
  if (badgeItem.badgeType == BadgeType::kBadgeTypeIncognito) {
    self.fullScreenBadge = newButton;
    return;
  }
  if (!self.badges) {
    self.badges = [[NSMutableArray alloc] init];
  }
  // No need to animate this change since it is the initial state.
  [newButton setAccepted:badgeItem.accepted animated:NO];
  [self.badges addObject:newButton];
  [self updateDisplayedBadges];
}

- (void)removeBadge:(id<BadgeItem>)badgeItem {
  for (BadgeButton* button in self.badges) {
    if (button.badgeType == badgeItem.badgeType) {
      [self.badges removeObject:button];
      break;
    }
  }
  [self updateDisplayedBadges];
}

- (void)updateBadge:(id<BadgeItem>)badgeItem {
  for (BadgeButton* button in self.badges) {
    if (button.badgeType == badgeItem.badgeType) {
      [button setAccepted:badgeItem.accepted animated:YES];
      return;
    }
  }
}

#pragma mark FullscreenUIElement

- (void)updateForFullscreenProgress:(CGFloat)progress {
  CGFloat alphaValue = fmax((progress - 0.85) / 0.15, 0);
  self.displayedBadge.alpha = alphaValue;
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
