// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_COOKIES_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_COOKIES_DELEGATE_H_

// Delegate related to Cookies state.
@protocol PageInfoCookiesDelegate

// Blocks or allows Third-Party Cookies.
- (void)blockThirdPartyCookies:(BOOL)blocked;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_COOKIES_DELEGATE_H_
