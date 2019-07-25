// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/badges/badge_button_factory.h"

#import "base/logging.h"
#import "ios/chrome/browser/ui/badges/badge_button.h"
#import "ios/chrome/browser/ui/badges/badge_button_action_handler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BadgeButtonFactory ()

// Action handlers for the buttons.
@property(nonatomic, strong) BadgeButtonActionHandler* actionHandler;

@end

@implementation BadgeButtonFactory

- (instancetype)initWithActionHandler:(BadgeButtonActionHandler*)actionHandler {
  self = [super init];
  if (self) {
    _actionHandler = self.actionHandler;
  }
  return self;
}

- (BadgeButton*)getBadgeButtonForBadgeType:(BadgeType)badgeType {
  switch (badgeType) {
    case BadgeType::kBadgeTypePasswordSave:
      return [self passwordsSaveBadgeButton];
      break;
    case BadgeType::kBadgeTypePasswordUpdate:
      return [self passwordsUpdateBadgeButton];
    case BadgeType::kBadgeTypeNone:
      NOTREACHED() << "A badge should not have kBadgeTypeNone";
      return nil;
  }
}

#pragma mark - Private

- (BadgeButton*)passwordsSaveBadgeButton {
  BadgeButton* button =
      [self createButtonForType:BadgeType::kBadgeTypePasswordSave
                     imageNamed:@"infobar_passwords_icon"];
  [button addTarget:self.actionHandler
                action:@selector(passwordsBadgeButtonTapped:)
      forControlEvents:UIControlEventTouchUpInside];
  return button;
}

- (BadgeButton*)passwordsUpdateBadgeButton {
  BadgeButton* button =
      [self createButtonForType:BadgeType::kBadgeTypePasswordUpdate
                     imageNamed:@"infobar_passwords_icon"];
  [button addTarget:self.actionHandler
                action:@selector(passwordsBadgeButtonTapped:)
      forControlEvents:UIControlEventTouchUpInside];
  return button;
}

- (BadgeButton*)createButtonForType:(BadgeType)badgeType
                         imageNamed:(NSString*)imageName {
  BadgeButton* button = [BadgeButton badgeButtonWithType:badgeType];
  UIImage* image = [[UIImage imageNamed:imageName]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  [button setImage:image forState:UIControlStateNormal];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.imageView.contentMode = UIViewContentModeScaleAspectFit;
  return button;
}

@end
