// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WHATS_NEW_DEFAULT_BROWSER_UTILS_H_
#define IOS_CHROME_BROWSER_UI_WHATS_NEW_DEFAULT_BROWSER_UTILS_H_

// Logs the timestamp of user activity that is deemed to be an indication of
// a user that would likely benefit from having Chrome set as their default
// browser. Before logging the current activity, this method will also clear all
// past expired logs that have happened too far in the past.
void LogLikelyInterestedDefaultBrowserUserActivity();

#endif  // IOS_CHROME_BROWSER_UI_WHATS_NEW_DEFAULT_BROWSER_UTILS_H_
