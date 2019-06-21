// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_UTIL_UICOLOR_CR_SEMANTIC_COLORS_H_
#define IOS_CHROME_COMMON_UI_UTIL_UICOLOR_CR_SEMANTIC_COLORS_H_

#import <UIKit/UIKit.h>

// This category wraps the Apple-provided semantic colors because many of them
// are only available in iOS 13. Only these wrapper functions should be added
// to this file. Custom dynamic colors should go in ColorSets.
@interface UIColor (CRSemanticColors)

@property(class, nonatomic, readonly)
    UIColor* cr_secondarySystemGroupedBackgroundColor;

@end

#endif  // IOS_CHROME_COMMON_UI_UTIL_UICOLOR_CR_SEMANTIC_COLORS_H_
