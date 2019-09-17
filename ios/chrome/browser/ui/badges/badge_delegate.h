// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BADGES_BADGE_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_BADGES_BADGE_DELEGATE_H_

// Protocol to communicate Badge actions to the coordinator.
@protocol BadgeDelegate
// Shows the badge overflow menu.
- (void)showOverflowMenu;
@end

#endif  // IOS_CHROME_BROWSER_UI_BADGES_BADGE_DELEGATE_H_
