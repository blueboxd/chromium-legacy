// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITIES_BOOKMARK_ACTIVITY_H_
#define IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITIES_BOOKMARK_ACTIVITY_H_

#import <UIKit/UIKit.h>

namespace bookmarks {
class BookmarkModel;
}

@protocol BrowserCommands;
class GURL;
class PrefService;

// Activity that adds the page to bookmarks.
@interface BookmarkActivity : UIActivity

// Initializes the bookmark activity with the |URL| to check to know if the page
// is already bookmarked in the |bookmarkModel|. The |handler| is used to add
// the page to the bookmarks. The |prefService| is used to verify if the user
// can edit their bookmarks or not.
- (instancetype)initWithURL:(const GURL&)URL
              bookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
                    handler:(id<BrowserCommands>)handler
                prefService:(PrefService*)prefService;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITIES_BOOKMARK_ACTIVITY_H_
