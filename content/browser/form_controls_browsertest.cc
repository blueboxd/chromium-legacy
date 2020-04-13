// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/screenshot_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "ui/base/ui_base_features.h"

// TODO(crbug.com/958242): Move the baselines to skia gold for easier
//   rebaselining when all platforms are supported.

// To rebaseline this test on all platforms:
// 1. Run a CQ+1 dry run.
// 2. Click the failing bots for android, windows, mac, and linux.
// 3. Find the failing interactive_ui_browsertests step.
// 4. Click the "Deterministic failure" link for the failing test case.
// 5. Copy the "Actual pixels" data url and paste into browser.
// 6. Save the image into your chromium checkout in content/test/data/forms/.

namespace content {

class FormControlsBrowserTest : public ContentBrowserTest {
 public:
  FormControlsBrowserTest() {
    feature_list_.InitWithFeatures({features::kFormControlsRefresh}, {});
  }

  void SetUp() override {
    EnablePixelOutput();
    ContentBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    SetUpCommandLineForScreenshotTest(command_line);
  }

  void RunTest(const std::string& screenshot_filename,
               const std::string& body_html,
               int screenshot_width,
               int screenshot_height) {
    base::ScopedAllowBlockingForTesting allow_blocking;

    ASSERT_TRUE(features::IsFormControlsRefreshEnabled());

    base::FilePath dir_test_data;
    ASSERT_TRUE(base::PathService::Get(DIR_TEST_DATA, &dir_test_data));
    base::FilePath golden_screenshot_filepath =
        dir_test_data.AppendASCII("forms").AppendASCII(screenshot_filename +
                                                       ".png");

    ASSERT_TRUE(NavigateToURL(
        shell()->web_contents(),
        GURL("data:text/html,<!DOCTYPE html><body>" + body_html + "</body>")));

    RunScreenshotTest(shell()->web_contents(), golden_screenshot_filepath,
                      screenshot_width, screenshot_height);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(FormControlsBrowserTest, Checkbox) {
  RunTest("form_controls_browsertest_checkbox",
          "<input type=checkbox>"
          "<input type=checkbox checked>"
          "<input type=checkbox disabled>"
          "<input type=checkbox checked disabled>"
          "<input type=checkbox id=\"indeterminate\">"
          "<script>"
          "  document.getElementById('indeterminate').indeterminate = true"
          "</script>",
          /* screenshot_width */ 130,
          /* screenshot_height */ 40);
}

IN_PROC_BROWSER_TEST_F(FormControlsBrowserTest, Radio) {
  RunTest("form_controls_browsertest_radio",
          "<input type=radio>"
          "<input type=radio checked>"
          "<input type=radio disabled>"
          "<input type=radio checked disabled>"
          "<input type=radio id=\"indeterminate\">"
          "<script>"
          "  document.getElementById('indeterminate').indeterminate = true"
          "</script>",
          /* screenshot_width */ 140,
          /* screenshot_height */ 40);
}

// TODO(jarhar): Add tests for other elements from
//   https://concrete-hardboard.glitch.me

}  // namespace content
