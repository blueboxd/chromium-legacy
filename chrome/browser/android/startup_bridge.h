// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_STARTUP_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_STARTUP_BRIDGE_H_

namespace android_startup {
extern void LoadFullBrowser();

// Called to start up java bits of chrome synchronously after the C++ bits have
// been initialized. Used for browser tests which must run the browser
// synchronously before running the test.
extern void HandlePostNativeStartupSynchronously();
}

#endif  // CHROME_BROWSER_ANDROID_STARTUP_BRIDGE_H_
