// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/badges/badge_view_controller.h"

#import "ios/chrome/browser/ui/badges/badge_button.h"
#import "ios/chrome/browser/ui/badges/badge_button_action_handler.h"
#import "ios/chrome/browser/ui/badges/badge_button_factory.h"
#import "ios/chrome/browser/ui/badges/badge_item.h"
#import "ios/chrome/browser/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_element.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BadgeViewController () <FullscreenUIElement>

// Button factory.
@property(nonatomic, strong) BadgeButtonFactory* buttonFactory;

// Badge button to show when FullScreen is in expanded mode (i.e. when the
// toolbars are expanded). Setting this property will add the button to the
// view hierarchy.
@property(nonatomic, strong) BadgeButton* displayedBadge;

// Array of all available badges.
@property(nonatomic, strong) NSMutableArray<BadgeButton*>* badges;

@end

@implementation BadgeViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  BadgeButtonActionHandler* actionHandler =
      [[BadgeButtonActionHandler alloc] init];
  actionHandler.dispatcher = self.dispatcher;
  self.buttonFactory =
      [[BadgeButtonFactory alloc] initWithActionHandler:actionHandler];
}

#pragma mark - Protocols

#pragma mark BadgeConsumer

- (void)setupWithBadges:(NSArray*)badges {
  if (!self.badges) {
    self.badges = [[NSMutableArray alloc] init];
  }
  [self.displayedBadge removeFromSuperview];
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
  if (!self.badges) {
    self.badges = [[NSMutableArray alloc] init];
  }
  BadgeButton* newButton =
      [self.buttonFactory getBadgeButtonForBadgeType:badgeItem.badgeType];
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
  [_displayedBadge removeFromSuperview];
  if (!badgeButton) {
    _displayedBadge = nil;
    return;
  }

  _displayedBadge = badgeButton;
  [self.view addSubview:_displayedBadge];
  [NSLayoutConstraint activateConstraints:@[
    [_displayedBadge.widthAnchor
        constraintEqualToAnchor:_displayedBadge.heightAnchor],
    [_displayedBadge.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [_displayedBadge.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    [_displayedBadge.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [_displayedBadge.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
  ]];
}

#pragma mark - Helpers

- (void)updateDisplayedBadges {
  self.displayedBadge = [self.badges lastObject];
}

@end
