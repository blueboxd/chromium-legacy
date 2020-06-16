// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_PASSWORD_CHECK_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_PASSWORD_CHECK_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"

// SettingsSwitchItem is a model class that uses SettingsSwitchCell.
@interface SettingsPasswordCheckItem : TableViewItem

// The text to display.
@property(nonatomic, copy) NSString* text;

// The detail text string.
@property(nonatomic, copy) NSString* detailText;

// The image to display (required). If this image should be tinted to match the
// text color (e.g. in dark mode), the provided image should have rendering mode
// UIImageRenderingModeAlwaysTemplate. Don't set image with |isIndicatorHidden|
// as only either image or |activityIndicator| will be shown.
@property(nonatomic, strong) UIImage* image;

// Tint color for an image.
@property(nonatomic, copy) UIColor* tintColor;

// Controls visibility of |activityIndicator|, if set true |imageView| will be
// hidden and activity indicator will be shown. In case both image is provided
// and this property set to false, only |activityIndicator| will be shown.
@property(nonatomic, assign, getter=isIndicatorHidden) BOOL indicatorHidden;

// Disabled cell are automatically drawn with dimmed text and without
// |imageView| or |activityIndicator|.
@property(nonatomic, assign, getter=isEnabled) BOOL enabled;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_PASSWORD_CHECK_ITEM_H_
