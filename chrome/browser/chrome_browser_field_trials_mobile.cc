// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_field_trials_mobile.h"

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"

#if defined(OS_ANDROID)
#include "components/prerender/browser/prerender_field_trial.h"
#endif

namespace chrome {

void SetupMobileFieldTrials() {
  DCHECK(!g_browser_process);
#if defined(OS_ANDROID)
  prerender::ConfigureNoStatePrefetch();
#endif
}

}  // namespace chrome
