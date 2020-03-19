// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_OMNIBOX_FOCUSER_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_OMNIBOX_FOCUSER_H_

#import <Foundation/Foundation.h>

// This protocol provides callbacks for focusing the omnibox.
@protocol OmniboxFocuser
// Give focus to the omnibox, if it is visible. No-op if it is not visible.  If
// current page is an NTP, first focus the NTP fakebox.
- (void)focusOmnibox;
// Focus the omnibox but skip the NTP check.
- (void)focusOmniboxFromFakebox;
// Cancel omnibox edit (from shield tap or cancel button tap).
- (void)cancelOmniboxEdit;
@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_OMNIBOX_FOCUSER_H_
