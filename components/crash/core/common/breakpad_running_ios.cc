// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/crash/core/common/breakpad_running_ios.h"

namespace crash_reporter {

namespace {

bool g_breakpad_running;

}  // namespace

bool IsBreakpadRunning() {
  return g_breakpad_running;
}

void SetBreakpadRunning(bool running) {
  g_breakpad_running = running;
}

}  // namespace crash_reporter
