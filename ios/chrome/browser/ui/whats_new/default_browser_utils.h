// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WHATS_NEW_DEFAULT_BROWSER_UTILS_H_
#define IOS_CHROME_BROWSER_UI_WHATS_NEW_DEFAULT_BROWSER_UTILS_H_

#import <UIKit/UIKit.h>

// UserDefaults key that saves the last time an HTTP(S) link was sent and opened
// by the app.
extern NSString* const kLastHTTPURLOpenTime;

// Logs the timestamp of user activity that is deemed to be an indication of
// a user that would likely benefit from having Chrome set as their default
// browser. Before logging the current activity, this method will also clear all
// past expired logs that have happened too far in the past.
void LogLikelyInterestedDefaultBrowserUserActivity();

// Returns True if the last URL open is within the time threshold that would
// indicate Chrome is likely still the default browser. Returns False otherwise.
bool IsChromeLikelyDefaultBrowser();

#endif  // IOS_CHROME_BROWSER_UI_WHATS_NEW_DEFAULT_BROWSER_UTILS_H_
