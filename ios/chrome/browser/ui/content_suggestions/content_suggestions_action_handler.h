// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_ACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_ACTION_HANDLER_H_

// Action handler protocol for the content suggestions view controller.
@protocol ContentSuggestionsActionHandler
// Paginates the Discover feed by appending a set of articles to
// the bottom of it.
- (void)loadMoreFeedArticles;
@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_ACTION_HANDLER_H_
