// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/android/android_browser_test.h"

#include "chrome/browser/android/startup_bridge.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "content/public/test/test_utils.h"

AndroidBrowserTest::AndroidBrowserTest() = default;
AndroidBrowserTest::~AndroidBrowserTest() = default;

void AndroidBrowserTest::SetUp() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  SetUpCommandLine(command_line);
  SetUpDefaultCommandLine(command_line);

  BrowserTestBase::SetUp();
}

void AndroidBrowserTest::SetUpDefaultCommandLine(
    base::CommandLine* command_line) {
  test_launcher_utils::PrepareBrowserCommandLineForTests(command_line);
  test_launcher_utils::PrepareBrowserCommandLineForBrowserTests(
      command_line, /*open_about_blank_on_launch=*/true);
}

void AndroidBrowserTest::PreRunTestOnMainThread() {
  android_startup::HandlePostNativeStartupSynchronously();

  // Pump startup related events.
  content::RunAllPendingInMessageLoop();
}

void AndroidBrowserTest::PostRunTestOnMainThread() {
  // Sometimes tests leave Quit tasks in the MessageLoop (for shame), so let's
  // run all pending messages here to avoid preempting the QuitBrowsers tasks.
  // TODO(https://crbug.com/922118): Remove this once it is no longer possible
  // to post QuitCurrent* tasks.
  content::RunAllPendingInMessageLoop();
}
