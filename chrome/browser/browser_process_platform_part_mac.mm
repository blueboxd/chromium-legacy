// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process_platform_part_mac.h"

#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/chrome_browser_application_mac.h"
#include "components/metal_util/test_shader.h"

namespace {

void TestShaderCallback(metal::TestShaderResult result,
                        const base::TimeDelta& method_time,
                        const base::TimeDelta& compile_time) {
  switch (result) {
    case metal::TestShaderResult::kNotAttempted:
    case metal::TestShaderResult::kFailed:
      // Don't include data if no Metal device was created (e.g, due to hardware
      // or macOS version reasons).
      return;
    case metal::TestShaderResult::kTimedOut:
      DCHECK_EQ(compile_time, metal::kTestShaderTimeForever);
      break;
    case metal::TestShaderResult::kSucceeded:
      break;
  }
  UMA_HISTOGRAM_MEDIUM_TIMES("Browser.Metal.TestShaderMethodTime", method_time);
  UMA_HISTOGRAM_MEDIUM_TIMES("Browser.Metal.TestShaderCompileTime",
                             compile_time);
}

}  // namespace

BrowserProcessPlatformPart::BrowserProcessPlatformPart() {
}

BrowserProcessPlatformPart::~BrowserProcessPlatformPart() {
}

void BrowserProcessPlatformPart::StartTearDown() {
  app_shim_listener_ = nullptr;
}

void BrowserProcessPlatformPart::AttemptExit(bool try_to_quit_application) {
  // On the Mac, the application continues to run once all windows are closed.
  // Terminate will result in a CloseAllBrowsers() call, and once (and if)
  // that is done, will cause the application to exit cleanly.
  //
  // This function is called for two types of attempted exits: URL requests
  // (chrome://quit or chrome://restart), and a keyboard menu invocations of
  // command-Q. (Interestingly, selecting the Quit command with the mouse don't
  // come down this code path at all.) URL requests to exit have
  // |try_to_quit_application| set to true; keyboard menu invocations have it
  // set to false.

  if (!try_to_quit_application) {
    // A keyboard menu invocation.
    AppController* app_controller =
        base::mac::ObjCCastStrict<AppController>([NSApp delegate]);
    if (![app_controller runConfirmQuitPanel])
      return;
  }

  chrome_browser_application_mac::Terminate();
}

void BrowserProcessPlatformPart::PreMainMessageLoopRun() {
  // AppShimListener can not simply be reset, otherwise destroying the old
  // domain socket will cause the just-created socket to be unlinked.
  DCHECK(!app_shim_listener_.get());
  app_shim_listener_ = new AppShimListener;

  // Launch a test Metal shader compile once the run loop starts.
  metal::TestShader(base::BindOnce(&TestShaderCallback));
}

AppShimListener* BrowserProcessPlatformPart::app_shim_listener() {
  return app_shim_listener_.get();
}
