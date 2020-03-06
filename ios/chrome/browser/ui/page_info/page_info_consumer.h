// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_CONSUMER_H_

#import "ios/chrome/browser/ui/page_info/page_info_description.h"

// A consumer for page info changes.
@protocol PageInfoConsumer

// Called when the page info has changed.
- (void)pageInfoChanged:(PageInfoDescription*)pageInfoDescription;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_CONSUMER_H_
