// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DOWNLOAD_DOWNLOAD_STARTUP_UTILS_H_
#define CHROME_BROWSER_ANDROID_DOWNLOAD_DOWNLOAD_STARTUP_UTILS_H_

// Native side of DownloadStartupUtils.java.
class DownloadStartupUtils {
 public:
  static void EnsureDownloadSystemInitialized(bool is_full_browser_started);
};

#endif  // CHROME_BROWSER_ANDROID_DOWNLOAD_DOWNLOAD_STARTUP_UTILS_H_
