// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_SNIPPETS_CONTENT_SUGGESTIONS_SCHEDULER_NOTIFICATIONS_H_
#define IOS_CHROME_BROWSER_NTP_SNIPPETS_CONTENT_SUGGESTIONS_SCHEDULER_NOTIFICATIONS_H_

#import <Foundation/Foundation.h>

class ChromeBrowserState;

// Notify the scheduler of the Content Suggestions services of the app lifecycle
// events.
@interface ContentSuggestionsSchedulerNotifications : NSObject

// Notifies that the application is launching from cold state.
+ (void)notifyColdStart:(ChromeBrowserState*)browserState;
// Notifies that the application has been foregrounded.
+ (void)notifyForeground:(ChromeBrowserState*)browserState;

@end

#endif  // IOS_CHROME_BROWSER_NTP_SNIPPETS_CONTENT_SUGGESTIONS_SCHEDULER_NOTIFICATIONS_H_
