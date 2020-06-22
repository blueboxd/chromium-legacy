// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#ifndef IOS_CHROME_BROWSER_SCREENSHOT_SCREENSHOT_NOTIFICATION_LISTENER_H_
#define IOS_CHROME_BROWSER_SCREENSHOT_SCREENSHOT_NOTIFICATION_LISTENER_H_

// ScreenshotNotificationListener presents the public interface for
// the screenshot metric collection.
@interface ScreenshotNotificationListener : NSObject

// Adds an observer to the NSNotification centre, and listens for
// the UIApplicationUserDidTakeScreenshotNotification to trigger
// metric collection.
- (void)startListening;

@end

#endif  // IOS_CHROME_BROWSER_SCREENSHOT_SCREENSHOT_NOTIFICATION_LISTENER_H_
