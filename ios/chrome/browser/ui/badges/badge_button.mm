// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/badges/badge_button.h"

#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Duration of button animations, in seconds.
const CGFloat kButtonAnimationDuration = 0.2;
// Edge insets of button.
const CGFloat kButtonEdgeInset = 6;
// To achieve a circular corner radius, divide length of a side by 2.
const CGFloat kButtonCircularCornerRadiusDivisor = 2.0;
// Alpha value of button in an inactive state.
const CGFloat kButtonInactiveAlpha = 0.38;
}  // namespace

@interface BadgeButton ()

// Read/Write override.
@property(nonatomic, assign, readwrite) BadgeType badgeType;

@end

@implementation BadgeButton

+ (instancetype)badgeButtonWithType:(BadgeType)badgeType {
  BadgeButton* button = [self buttonWithType:UIButtonTypeSystem];
  button.badgeType = badgeType;

  return button;
}

- (void)willMoveToSuperview:(UIView*)newSuperview {
  self.imageEdgeInsets = UIEdgeInsetsMake(kButtonEdgeInset, kButtonEdgeInset,
                                          kButtonEdgeInset, kButtonEdgeInset);
  [super willMoveToSuperview:newSuperview];
}

- (void)layoutSubviews {
  [super layoutSubviews];
  self.layer.cornerRadius =
      self.bounds.size.height / kButtonCircularCornerRadiusDivisor;
}

- (void)setAccepted:(BOOL)accepted animated:(BOOL)animated {
  void (^changeTintColor)() = ^{
    self.tintColor = accepted ? [UIColor colorNamed:kTintColor]
                              : [UIColor colorWithWhite:0
                                                  alpha:kButtonInactiveAlpha];
  };
  if (animated) {
    [UIView animateWithDuration:kButtonAnimationDuration
                     animations:changeTintColor];
  } else {
    changeTintColor();
  }
}

@end
