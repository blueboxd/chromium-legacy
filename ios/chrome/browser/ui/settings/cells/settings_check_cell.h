// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_CHECK_CELL_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_CHECK_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/cells/table_view_cell.h"

// Cell representation for SettingsCheckItem.
//  +---------------------------------------------------------+
//  |                                           +--------+    |
//  | +--------+                                |trailing|    |
//  | | leading|  One line title                |image or|    |
//  | | image  |  Multiline detail text         |spinner |    |
//  | +--------+                                +--------+    |
//  +---------------------------------------------------------+
@interface SettingsCheckCell : TableViewCell

// Shows |activityIndicator| and starts animation. It will hide |imageView| if
// it was shown.
- (void)showActivityIndicator;

// Hides |activityIndicator| and stops animation.
- (void)hideActivityIndicator;

// Sets the |trailingImage| and tint |trailingColor| for it that should be
// displayed at the trailing edge of the cell. If set to nil, |trailingImage|
// will be hidden, otherwise |imageView| will be shown and |activityIndicator|
// will be hidden.
- (void)setTrailingImage:(UIImage*)trailingImage
           withTintColor:(UIColor*)tintColor;

// Sets the [leadingImage] and tint [leadingColor] for it that should be
// displayed at the leading edge of the cell.  If set to nil, the image is
// hidden.
- (void)setLeadingImage:(UIImage*)leadingImage
          withTintColor:(UIColor*)tintColor;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_CHECK_CELL_H_
