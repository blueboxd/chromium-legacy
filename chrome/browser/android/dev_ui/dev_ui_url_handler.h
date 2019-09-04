// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DEV_UI_DEV_UI_URL_HANDLER_H_
#define CHROME_BROWSER_ANDROID_DEV_UI_DEV_UI_URL_HANDLER_H_

#include "build/build_config.h"
#include "chrome/android/features/dev_ui/buildflags.h"

#if !defined(OS_ANDROID) || !BUILDFLAG(DFMIFY_DEV_UI)
#error Unsupported platform.
#endif

class GURL;

namespace content {
class BrowserContext;
}

namespace chrome {
namespace android {

// Handles chrome:// hosts for pages in the Developer UI Dynamic Feature Module
// (DevUI DFM). If not installed or not loaded, then replaces |url| with the
// chrome:// URL to the DevUI loader to install and load the DevUI DFM.
bool HandleDfmifiedDevUiPageURL(GURL* url,
                                content::BrowserContext* /* browser_context */);

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_DEV_UI_DEV_UI_URL_HANDLER_H_
