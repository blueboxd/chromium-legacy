// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/badges/sc_badge_coordinator.h"

#include "ios/chrome/browser/infobars/infobar_badge_model.h"
#import "ios/chrome/browser/ui/badges/badge_button_factory.h"
#import "ios/chrome/browser/ui/badges/badge_consumer.h"
#import "ios/chrome/browser/ui/badges/badge_static_item.h"
#import "ios/chrome/browser/ui/badges/badge_view_controller.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BadgeContainerViewController : UIViewController
@property(nonatomic, strong) BadgeViewController* centeredChildViewController;
@end

@implementation BadgeContainerViewController
- (void)viewDidLoad {
  [super viewDidLoad];
  [self addChildViewController:self.centeredChildViewController];
  [self.view addSubview:self.centeredChildViewController.view];
  [self didMoveToParentViewController:self.centeredChildViewController];
  self.centeredChildViewController.view
      .translatesAutoresizingMaskIntoConstraints = NO;
  AddSameCenterConstraints(self.centeredChildViewController.view, self.view);
  [NSLayoutConstraint activateConstraints:@[
    [self.centeredChildViewController.view.widthAnchor
        constraintGreaterThanOrEqualToConstant:1],
    [self.centeredChildViewController.view.heightAnchor
        constraintEqualToConstant:100]
  ]];
  UIView* containerView = self.view;
  containerView.backgroundColor = [UIColor whiteColor];
  self.title = @"Badges";
}

@end

@interface SCBadgeCoordinator ()
@property(nonatomic, strong)
    BadgeContainerViewController* containerViewController;
@property(nonatomic, strong) BadgeViewController* badgeViewController;
@property(nonatomic, weak) id<BadgeConsumer> consumer;
@end

@implementation SCBadgeCoordinator
@synthesize baseViewController = _baseViewController;

- (void)start {
  self.containerViewController = [[BadgeContainerViewController alloc] init];
  BadgeButtonFactory* buttonFactory =
      [[BadgeButtonFactory alloc] initWithActionHandler:nil];
  self.badgeViewController =
      [[BadgeViewController alloc] initWithButtonFactory:buttonFactory];
  self.consumer = self.badgeViewController;
  self.containerViewController.centeredChildViewController =
      self.badgeViewController;
  [self.baseViewController pushViewController:self.containerViewController
                                     animated:YES];
  BadgeStaticItem* incognitoItem = [[BadgeStaticItem alloc]
      initWithBadgeType:BadgeType::kBadgeTypeIncognito];
  InfobarBadgeModel* passwordBadgeItem = [[InfobarBadgeModel alloc]
      initWithInfobarType:InfobarType::kInfobarTypePasswordSave];
  [self.consumer setupWithDisplayedBadge:passwordBadgeItem
                         fullScreenBadge:incognitoItem];
}

@end
