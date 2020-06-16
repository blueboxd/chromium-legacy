// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_PASSWORD_CHECK_CELL_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_PASSWORD_CHECK_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/cells/table_view_cell.h"

// Cell representation for SettingsPasswordCheckItem.
//  +----------------------------------------------+
//  |                                 +-------+    |
//  |  One line title                 |icon or|    |
//  |  Multiline detail text          |spinner|    |
//  |                                 +-------+    |
//  +----------------------------------------------+
@interface SettingsPasswordCheckCell : TableViewCell

// Shows |activityIndicator| and starts animation. It will hide |imageView| if
// it was shown.
- (void)showActivityIndicator;

// Hides |activityIndicator| and stops animation.
- (void)hideActivityIndicator;

// Sets the icon |image| and tint |color| for it that should be displayed at the
// trailing edge of the cell. If set to nil, the icon will be hidden, otherwise
// |imageView| will be shown and |activityIndicator| will be hidden.
- (void)setIconImage:(UIImage*)image withTintColor:(UIColor*)color;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_PASSWORD_CHECK_CELL_H_
