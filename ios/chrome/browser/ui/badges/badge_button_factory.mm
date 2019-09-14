// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/badges/badge_button_factory.h"

#import "base/logging.h"
#import "ios/chrome/browser/ui/badges/badge_button.h"
#import "ios/chrome/browser/ui/badges/badge_button_action_handler.h"
#import "ios/chrome/browser/ui/badges/badge_constants.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

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
    _actionHandler = actionHandler;
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
    case BadgeType::kBadgeTypeIncognito:
      return [self incognitoBadgeButton];
    case BadgeType::kBadgeTypeNone:
      NOTREACHED() << "A badge should not have kBadgeTypeNone";
      return nil;
  }
}

#pragma mark - Private

- (BadgeButton*)passwordsSaveBadgeButton {
  BadgeButton* button =
      [self createButtonForType:BadgeType::kBadgeTypePasswordSave
                     imageNamed:@"infobar_passwords_icon"
                  renderingMode:UIImageRenderingModeAlwaysTemplate];
  [button addTarget:self.actionHandler
                action:@selector(passwordsBadgeButtonTapped:)
      forControlEvents:UIControlEventTouchUpInside];
  button.accessibilityIdentifier =
      kBadgeButtonSavePasswordAccessibilityIdentifier;
  button.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_INFOBAR_BADGES_PASSWORD_HINT);
  return button;
}

- (BadgeButton*)passwordsUpdateBadgeButton {
  BadgeButton* button =
      [self createButtonForType:BadgeType::kBadgeTypePasswordUpdate
                     imageNamed:@"infobar_passwords_icon"
                  renderingMode:UIImageRenderingModeAlwaysTemplate];
  [button addTarget:self.actionHandler
                action:@selector(passwordsBadgeButtonTapped:)
      forControlEvents:UIControlEventTouchUpInside];
  button.accessibilityIdentifier =
      kBadgeButtonUpdatePasswordAccessibilityIdentifier;
  button.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_INFOBAR_BADGES_PASSWORD_HINT);
  return button;
}

- (BadgeButton*)incognitoBadgeButton {
  BadgeButton* button =
      [self createButtonForType:BadgeType::kBadgeTypeIncognito
                     imageNamed:@"incognito_badge"
                  renderingMode:UIImageRenderingModeAlwaysOriginal];
  button.fullScreenImage = [[UIImage imageNamed:@"incognito_small_badge"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  button.tintColor = [UIColor colorNamed:kTextPrimaryColor];
  button.accessibilityTraits &= ~UIAccessibilityTraitButton;
  button.enabled = NO;
  button.accessibilityIdentifier = kBadgeButtonIncognitoAccessibilityIdentifier;
  button.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_BADGE_INCOGNITO_HINT);
  return button;
}

- (BadgeButton*)createButtonForType:(BadgeType)badgeType
                         imageNamed:(NSString*)imageName
                      renderingMode:(UIImageRenderingMode)renderingMode {
  BadgeButton* button = [BadgeButton badgeButtonWithType:badgeType];
  UIImage* image =
      [[UIImage imageNamed:imageName] imageWithRenderingMode:renderingMode];
  button.image = image;
  button.fullScreenOn = NO;
  button.imageView.contentMode = UIViewContentModeScaleAspectFit;
  [NSLayoutConstraint
      activateConstraints:@[ [button.widthAnchor
                              constraintEqualToAnchor:button.heightAnchor] ]];
  return button;
}

@end
