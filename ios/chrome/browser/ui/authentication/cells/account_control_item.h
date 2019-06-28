// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_CELLS_ACCOUNT_CONTROL_ITEM_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_CELLS_ACCOUNT_CONTROL_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"

// Item for account collection view and sign-in confirmation view.
@interface AccountControlItem : TableViewItem

// If this image should be tinted to match the text color (e.g. in dark mode),
// the provided image should have rendering mode
// UIImageRenderingModeAlwaysTemplate.
@property(nonatomic, strong) UIImage* image;
@property(nonatomic, copy) NSString* text;
@property(nonatomic, copy) NSString* detailText;
@property(nonatomic, assign) BOOL shouldDisplayError;


@end

// Cell for account settings view with a leading imageView, title text label,
// and detail text label. The imageView is top-leading aligned.
@interface AccountControlCell : TableViewCell

@property(nonatomic, readonly, strong) UIImageView* imageView;
@property(nonatomic, readonly, strong) UILabel* textLabel;
@property(nonatomic, readonly, strong) UILabel* detailTextLabel;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_CELLS_ACCOUNT_CONTROL_ITEM_H_
