// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/whats_new/default_browser_utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import <UIKit/UIKit.h>

namespace {
NSString* const kLastSignificantUserEvent = @"lastSignificantUserEvent";

// Time threshold before activity timestamps should be removed. Currently set to
// seven days.
const NSTimeInterval kUserActivityTimestampExpiration = 7 * 24 * 60 * 60;
}

void LogLikelyInterestedDefaultBrowserUserActivity() {
  NSMutableArray<NSDate*>* pastUserEvents =
      [[[NSUserDefaults standardUserDefaults]
          arrayForKey:kLastSignificantUserEvent] mutableCopy];
  if (pastUserEvents) {
    NSDate* sevenDaysAgoDate =
        [NSDate dateWithTimeIntervalSinceNow:-kUserActivityTimestampExpiration];
    // Clear all timestamps that occur later than 7 days ago.
    NSUInteger firstUnexpiredIndex =
        [pastUserEvents indexOfObjectPassingTest:^BOOL(
                            NSDate* date, NSUInteger idx, BOOL* stop) {
          return ([date laterDate:sevenDaysAgoDate] == date);
        }];
    if (firstUnexpiredIndex != NSNotFound && firstUnexpiredIndex > 0) {
      [pastUserEvents removeObjectsInRange:NSMakeRange(0, firstUnexpiredIndex)];
    }
    [pastUserEvents addObject:[NSDate date]];
  } else {
    pastUserEvents = [NSMutableArray arrayWithObject:[NSDate date]];
  }

  [[NSUserDefaults standardUserDefaults] setObject:pastUserEvents
                                            forKey:kLastSignificantUserEvent];
}
