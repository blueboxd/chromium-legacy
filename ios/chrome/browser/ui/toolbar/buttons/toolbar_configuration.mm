// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/incognito_color_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ToolbarConfiguration

@synthesize style = _style;

- (instancetype)initWithStyle:(ToolbarStyle)style {
  self = [super init];
  if (self) {
    _style = style;
  }
  return self;
}

- (UIColor*)NTPBackgroundColor {
  return color::IncognitoDynamicColor(
      self.style == INCOGNITO, ntp_home::kNTPBackgroundColor(),
      [UIColor colorNamed:kBackgroundDarkColor]);
}

- (UIColor*)backgroundColor {
  return color::IncognitoDynamicColor(
      self.style == INCOGNITO, [UIColor colorNamed:kBackgroundColor],
      [UIColor colorNamed:kBackgroundDarkColor]);
}

- (UIColor*)buttonsTintColor {
  return color::IncognitoDynamicColor(
      self.style == INCOGNITO, [UIColor colorNamed:@"tab_toolbar_button_color"],
      [UIColor colorNamed:@"tab_toolbar_button_color_incognito"]);
}

- (UIColor*)buttonsTintColorHighlighted {
  return color::IncognitoDynamicColor(
      self.style == INCOGNITO,
      [UIColor colorNamed:@"tab_toolbar_button_color_highlighted"],
      [UIColor colorNamed:@"tab_toolbar_button_color_highlighted_incognito"]);
}

- (UIColor*)buttonsSpotlightColor {
  return color::IncognitoDynamicColor(
      self.style == INCOGNITO,
      [UIColor colorNamed:@"tab_toolbar_button_halo_color"],
      [UIColor colorNamed:@"tab_toolbar_button_halo_color_incognito"]);
}

- (UIColor*)dimmedButtonsSpotlightColor {
  return color::IncognitoDynamicColor(
      self.style == INCOGNITO,
      [UIColor colorNamed:@"tab_toolbar_button_halo_color"],
      [UIColor colorNamed:@"tab_toolbar_button_halo_color_incognito"]);
}

- (UIColor*)locationBarBackgroundColorWithVisibility:(CGFloat)visibilityFactor {
  return color::IncognitoDynamicColor(
      self.style == INCOGNITO,
      [[UIColor colorNamed:kTextfieldBackgroundColor]
          colorWithAlphaComponent:visibilityFactor],
      [[UIColor colorNamed:kTextfieldBackgroundDarkColor]
          colorWithAlphaComponent:visibilityFactor]);
}

@end
