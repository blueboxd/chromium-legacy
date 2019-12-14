// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_STARTUP_UTILS_H_
#define CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_STARTUP_UTILS_H_

#include "base/macros.h"

class Profile;
class ProfileKey;

// Native side of DownloadStartupUtils.java.
class DownloadStartupUtils {
 public:
  // Ensures that the download system is initialized for the targeted profile.
  // If |profile| is null, reduced mode will be assumed. The returned value is
  // the ProfileKey that was used.
  static ProfileKey* EnsureDownloadSystemInitialized(Profile* profile);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(DownloadStartupUtils);
};

#endif  // CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_STARTUP_UTILS_H_
