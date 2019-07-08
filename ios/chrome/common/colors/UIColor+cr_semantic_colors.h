// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_COLORS_UICOLOR_CR_SEMANTIC_COLORS_H_
#define IOS_CHROME_COMMON_COLORS_UICOLOR_CR_SEMANTIC_COLORS_H_

#import <UIKit/UIKit.h>

// This category wraps the Apple-provided semantic colors because many of them
// are only available in iOS 13. Only these wrapper functions should be added
// to this file. Custom dynamic colors should go in ColorSets.
// TODO (crbug.com/981889): Remove along with iOS 12.
@interface UIColor (CRSemanticColors)

// System Background Color
@property(class, nonatomic, readonly) UIColor* cr_systemBackgroundColor;
@property(class, nonatomic, readonly)
    UIColor* cr_secondarySystemBackgroundColor;

// System Grouped Background Colors
@property(class, nonatomic, readonly) UIColor* cr_systemGroupedBackgroundColor;
@property(class, nonatomic, readonly)
    UIColor* cr_secondarySystemGroupedBackgroundColor;

// Label Colors
@property(class, nonatomic, readonly) UIColor* cr_labelColor;
@property(class, nonatomic, readonly) UIColor* cr_secondaryLabelColor;

// Separator Colors
@property(class, nonatomic, readonly) UIColor* cr_opaqueSeparatorColor;

@end

#endif  // IOS_CHROME_COMMON_COLORS_UICOLOR_CR_SEMANTIC_COLORS_H_
