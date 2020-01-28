// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_SESSION_WINDOW_RESTORING_H_
#define IOS_CHROME_BROWSER_SESSIONS_SESSION_WINDOW_RESTORING_H_

@class SessionWindowIOS;

// API for an object that can save and restore session windows. Typically an
// object confoming to this protocol is responsible for a list of tabs, and
// calls to these methopds affect that list.
@protocol SessionWindowRestoring

// Restores the |window| (for example, after a crash). If there is only one tab,
// showing the NTP, then this tab should be clobbered, otherwise, the tabs from
// the restored sessions should be added at the end of the current list of tabs.
// If |initialRestore| is YES, this method does not log metrics for operations
// conducted during the restore and must be called before any observers are
// registered on the TabModel.  Returns YES if the single NTP tab is closed.
- (BOOL)restoreSessionWindow:(SessionWindowIOS*)window
           forInitialRestore:(BOOL)initialRestore;

@end

#endif  // IOS_CHROME_BROWSER_SESSIONS_SESSION_WINDOW_RESTORING_H_
