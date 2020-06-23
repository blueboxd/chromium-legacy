// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_DISCOVER_ITEM_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_DISCOVER_ITEM_H_

#import <UIKit/UIKit.h>

#import <MaterialComponents/MaterialCollectionCells.h>

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/suggested_content.h"

@class ContentSuggestionsDiscoverCell;

// Item displaying entire Discover feed collection view.
@interface ContentSuggestionsDiscoverItem
    : CollectionViewItem <SuggestedContent>

// Contains the Discover feed coming from Discover provider.
@property(nonatomic, weak) UIViewController* discoverFeed;

@end

// Associated cell, displaying the Discover feed adjusted to prevent nested
// scrolling.
@interface ContentSuggestionsDiscoverCell : MDCCollectionViewCell

// Height of Discover feed content to stretch the containing cell, in order to
// avoid nested scrolling.
- (CGFloat)feedHeight;

// Sets the cell content as the Discover feed's view.
- (void)setDiscoverFeedView:(UIViewController*)discoverFeed;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_DISCOVER_ITEM_H_
