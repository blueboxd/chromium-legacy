// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
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
  // This compiler guard is needed because the API to force dark colors in
  // incognito also needs it. Without this, incognito would appear in light
  // colors on iOS 13 before compiling with the iOS 13 SDK.
#if defined(__IPHONE_13_0) && (__IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_13_0)
  if (@available(iOS 13, *)) {
    return ntp_home::kNTPBackgroundColor();
  }
#endif
  switch (self.style) {
    case NORMAL:
      return ntp_home::kNTPBackgroundColor();
    case INCOGNITO:
      return [UIColor colorWithWhite:kNTPBackgroundColorBrightnessIncognito
                               alpha:1.0];
  }
}

- (UIColor*)backgroundColor {
  // This compiler guard is needed because the API to force dark colors in
  // incognito also needs it. Without this, incognito would appear in light
  // colors on iOS 13 before compiling with the iOS 13 SDK.
#if defined(__IPHONE_13_0) && (__IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_13_0)
  if (@available(iOS 13, *)) {
    return [UIColor colorNamed:kBackgroundColor];
  }
#endif
  switch (self.style) {
    case NORMAL:
      return [UIColor colorNamed:kBackgroundColor];
    case INCOGNITO:
      return UIColorFromRGB(kIncognitoToolbarBackgroundColor);
  }
}

- (UIColor*)buttonsTintColor {
  // This compiler guard is needed because the API to force dark colors in
  // incognito also needs it. Without this, incognito would appear in light
  // colors on iOS 13 before compiling with the iOS 13 SDK.
#if defined(__IPHONE_13_0) && (__IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_13_0)
  if (@available(iOS 13, *)) {
    return [UIColor colorNamed:@"tab_toolbar_button_color"];
  }
#endif
  switch (self.style) {
    case NORMAL:
      return [UIColor colorNamed:@"tab_toolbar_button_color"];
    case INCOGNITO:
      return [UIColor whiteColor];
  }
}

- (UIColor*)buttonsTintColorHighlighted {
  // This compiler guard is needed because the API to force dark colors in
  // incognito also needs it. Without this, incognito would appear in light
  // colors on iOS 13 before compiling with the iOS 13 SDK.
#if defined(__IPHONE_13_0) && (__IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_13_0)
  if (@available(iOS 13, *)) {
    return [UIColor colorNamed:@"tab_toolbar_button_color_highlighted"];
  }
#endif
  switch (self.style) {
    case NORMAL:
      return [UIColor colorNamed:@"tab_toolbar_button_color_highlighted"];
    case INCOGNITO:
      return [UIColor
          colorWithWhite:1
                   alpha:kIncognitoToolbarButtonTintColorAlphaHighlighted];
  }
}

- (UIColor*)buttonsSpotlightColor {
  // This compiler guard is needed because the API to force dark colors in
  // incognito also needs it. Without this, incognito would appear in light
  // colors on iOS 13 before compiling with the iOS 13 SDK.
#if defined(__IPHONE_13_0) && (__IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_13_0)
  if (@available(iOS 13, *)) {
    return [UIColor colorNamed:@"tab_toolbar_button_halo_color"];
  }
#endif
  switch (self.style) {
    case NORMAL:
      return [UIColor colorNamed:@"tab_toolbar_button_halo_color"];
    case INCOGNITO:
      return [UIColor colorWithWhite:1 alpha:kToolbarSpotlightAlpha];
  }
}

- (UIColor*)dimmedButtonsSpotlightColor {
  // This compiler guard is needed because the API to force dark colors in
  // incognito also needs it. Without this, incognito would appear in light
  // colors on iOS 13 before compiling with the iOS 13 SDK.
#if defined(__IPHONE_13_0) && (__IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_13_0)
  if (@available(iOS 13, *)) {
    return [UIColor colorNamed:@"tab_toolbar_button_halo_color"];
  }
#endif
  switch (self.style) {
    case NORMAL:
      return [UIColor colorNamed:@"tab_toolbar_button_halo_color"];
    case INCOGNITO:
      return [UIColor colorWithWhite:1 alpha:kDimmedToolbarSpotlightAlpha];
  }
}

- (UIColor*)locationBarBackgroundColorWithVisibility:(CGFloat)visibilityFactor {
  // This compiler guard is needed because the API to force dark colors in
  // incognito also needs it. Without this, incognito would appear in light
  // colors on iOS 13 before compiling with the iOS 13 SDK.
#if defined(__IPHONE_13_0) && (__IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_13_0)
  if (@available(iOS 13, *)) {
    return [[UIColor colorNamed:kTextfieldBackgroundColor]
        colorWithAlphaComponent:visibilityFactor];
  }
#endif
  switch (self.style) {
    case NORMAL:
      return [[UIColor colorNamed:kTextfieldBackgroundColor]
          colorWithAlphaComponent:visibilityFactor];
    case INCOGNITO:
      return
          [UIColor colorWithWhite:1
                            alpha:kAdaptiveLocationBarBackgroundAlphaIncognito *
                                  visibilityFactor];
  }
}

@end
