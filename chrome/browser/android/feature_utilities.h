// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FEATURE_UTILITIES_H_
#define CHROME_BROWSER_ANDROID_FEATURE_UTILITIES_H_

#include <jni.h>

#include <string>

namespace chrome {
namespace android {

bool IsDownloadAutoResumptionEnabledInNative();

// Returns a finch group name currently used for the reached code profiler.
// Returns an empty string if the group isn't specified.
std::string GetReachedCodeProfilerTrialGroup();

} // namespace android
} // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_FEATURE_UTILITIES_H_
