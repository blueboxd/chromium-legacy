// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_CONSUMER_H_

// Consumer protocol for the content suggestions view controller.
@protocol ContentSuggestionsConsumer
// Notifies the consumer to set the content suggestions enabled
// based on the user setting.
- (void)setContentSuggestionsEnabled:(BOOL)enabled;
@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_CONSUMER_H_
