// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DFM_RESOURCE_BUNDLE_HELPER_H_
#define CHROME_BROWSER_ANDROID_DFM_RESOURCE_BUNDLE_HELPER_H_

#include "chrome/android/features/dev_ui/buildflags.h"

namespace android {

#if BUILDFLAG(DFMIFY_DEV_UI)
// Loads the resources for DevUI pages. Performs file I/O, and should be called
// on a worker thread.
void LoadDevUiResources();
#endif  // BUILDFLAG(DFMIFY_DEV_UI)

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_DFM_RESOURCE_BUNDLE_HELPER_H_
