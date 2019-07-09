// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation UIColor (CRSemanticColors)

#pragma mark - System Background Colors

+ (UIColor*)cr_systemBackgroundColor {
#if defined(__IPHONE_13_0) && (__IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_13_0)
  if (@available(iOS 13, *)) {
    return UIColor.systemBackgroundColor;
  }
#endif
  return UIColor.whiteColor;
}

+ (UIColor*)cr_secondarySystemBackgroundColor {
#if defined(__IPHONE_13_0) && (__IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_13_0)
  if (@available(iOS 13, *)) {
    return UIColor.secondarySystemBackgroundColor;
  }
#endif
  return UIColor.whiteColor;
}

#pragma mark - System Grouped Background Colors

+ (UIColor*)cr_systemGroupedBackgroundColor {
#if defined(__IPHONE_13_0) && (__IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_13_0)
  if (@available(iOS 13, *)) {
    return UIColor.systemGroupedBackgroundColor;
  }
#endif
  return UIColor.groupTableViewBackgroundColor;
}

+ (UIColor*)cr_secondarySystemGroupedBackgroundColor {
#if defined(__IPHONE_13_0) && (__IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_13_0)
  if (@available(iOS 13, *)) {
    return UIColor.secondarySystemGroupedBackgroundColor;
  }
#endif
  return UIColor.whiteColor;
}

#pragma mark - Label Colors

+ (UIColor*)cr_labelColor {
#if defined(__IPHONE_13_0) && (__IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_13_0)
  if (@available(iOS 13, *)) {
    return UIColor.labelColor;
  }
#endif
  return UIColor.blackColor;
}

+ (UIColor*)cr_secondaryLabelColor {
#if defined(__IPHONE_13_0) && (__IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_13_0)
  if (@available(iOS 13, *)) {
    return UIColor.secondaryLabelColor;
  }
#endif
  // This is the value for UIColor.secondaryLabelColor in light mode.
  return [UIColor colorWithRed:0x3C / (CGFloat)0xFF
                         green:0x3C / (CGFloat)0xFF
                          blue:0x43 / (CGFloat)0xFF
                         alpha:0.6];
}

#pragma mark - Separator Colors

+ (UIColor*)cr_separatorColor {
#if defined(__IPHONE_13_0) && (__IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_13_0)
  if (@available(iOS 13, *)) {
    return UIColor.separatorColor;
  }
#endif
  // This is the value for separatorColor in light mode.
  return [UIColor colorWithRed:0x3C / (CGFloat)0xFF
                         green:0x3C / (CGFloat)0xFF
                          blue:0x43 / (CGFloat)0xFF
                         alpha:0.6];
}

+ (UIColor*)cr_opaqueSeparatorColor {
#if defined(__IPHONE_13_0) && (__IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_13_0)
  if (@available(iOS 13, *)) {
    return UIColor.opaqueSeparatorColor;
  }
#endif
  // This is the value for opaqueSeparatorColor in light mode.
  return [UIColor colorWithRed:0xC7 / (CGFloat)0xFF
                         green:0xC7 / (CGFloat)0xFF
                          blue:0xC7 / (CGFloat)0xFF
                         alpha:1];
}

@end
