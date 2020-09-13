// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/system_services.h"

#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>

#include "base/mac/mac_logging.h"

extern "C" {
OSStatus SetApplicationIsDaemon(Boolean isDaemon);
void _LSSetApplicationLaunchServicesServerConnectionStatus(
    uint64_t flags,
    bool (^connection_allowed)(CFDictionaryRef options));

// See
// https://github.com/WebKit/webkit/commit/8da694b0b3febcc262653d01a45e946ce91845ed.
void _CSCheckFixDisable() API_AVAILABLE(macosx(10.15));
}  // extern "C"

namespace sandbox {

void DisableLaunchServices() {
}

void DisableCoreServicesCheckFix() {
  if (__builtin_available(macOS 10.15, *)) {
    _CSCheckFixDisable();
  }
}

}  // namespace sandbox
