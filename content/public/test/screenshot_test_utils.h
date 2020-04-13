// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_SCREENSHOT_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_SCREENSHOT_TEST_UTILS_H_

#include <string>

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace content {

class WebContents;

// This file contains functions to help build browsertests which take
// screenshots of web content and make pixel comparisons to golden baseline
// images. While you might normally use web_tests to make pixel tests of web
// content, making a browsertest helps highlight platform specific differences
// not rendered in web_tests like the different rendering of focus rings.

// Adds command-line flags to help unify rendering across devices and
// platforms. This should be called in the SetUpCommandLine function of browser
// tests.
void SetUpCommandLineForScreenshotTest(base::CommandLine* command_line);

// Runs a screenshot test by taking a screenshot of the given |web_contents|
// and comparing it to a golden baseline image file.
//
// |golden_screenshot_filepath| is the filepath to the golden expected
// screenshot for the test to compare to. For platform-specific differences, a
// different file for that platform can be provided and will be used
// automatically if present and conforms to the correct naming scheme. If no
// such platform specific golden image is present, the "default" one without a
// platform specific extension will be used, which is always used for Linux.
// The KitKat Android bot tends to render differently enough from the other
// Android bot that it is tracked separately. If no kitkat golden image is
// provided, it will default to the Linux golden, like all other platforms.
// Here is an example of all of the golden files present for a test which
// renders differently on all platforms:
// my_screenshot_test.png
// my_screenshot_test_mac.png
// my_screenshot_test_win.png
// my_screenshot_test_chromeos.png
// my_screenshot_test_android.png
// my_screenshot_test_android_kitkat.png
void RunScreenshotTest(WebContents* web_contents,
                       const base::FilePath& golden_screenshot_filepath,
                       int screenshot_width,
                       int screenshot_height);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_SCREENSHOT_TEST_UTILS_H_
