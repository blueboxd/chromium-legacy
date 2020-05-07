// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_utils.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/skbitmap_operations.h"

// To rebaseline this test on android:
// 1. Run a CQ+1 dry run
// 2. Click the failing android bot
// 3. Find the failing content_browsertests step
// 4. Click the "Deterministic failure" link for the failing test case
// 5. Copy the "Actual pixels" data url and paste into browser
// 6. Save the image into your chromium checkout in content/test/data/forms/

namespace content {

class FormControlsBrowserTest : public ContentBrowserTest {
 public:
  FormControlsBrowserTest() = default;

  void SetUp() override {
    EnablePixelOutput();
    ContentBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    // The --force-device-scale-factor flag helps make the pixel output of
    // different android trybots more similar.
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "1.0");
    feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    feature_list_->InitWithFeatures({features::kFormControlsRefresh}, {});
  }

  void TearDown() override { feature_list_.reset(); }

  void AsyncSnapshotCallback(const gfx::Image& image) {
    got_snapshot_ = true;
    snapshot_ = image;
  }

  void RunFormControlsTest(const std::string& expected_filename,
                           const std::string& body_html,
                           int screenshot_width,
                           int screenshot_height) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(features::IsFormControlsRefreshEnabled());

    std::string url =
        "data:text/html,<!DOCTYPE html>"
        "<head>"
        // The <meta name=viewport> tag helps make the pixel output of
        // different android trybots more similar.
        "  <meta name=\"viewport\" content=\"width=640, initial-scale=1, "
        "    maximum-scale=1, minimum-scale=1\">"
        "</head>"
        "<body>" +
        body_html + "</body>";
    ASSERT_TRUE(NavigateToURL(shell(), GURL(url)));

    RenderWidgetHostImpl* const rwh =
        RenderWidgetHostImpl::From(shell()
                                       ->web_contents()
                                       ->GetRenderWidgetHostView()
                                       ->GetRenderWidgetHost());
    CHECK(rwh);
    rwh->GetSnapshotFromBrowser(
        base::BindOnce(&FormControlsBrowserTest::AsyncSnapshotCallback,
                       base::Unretained(this)),
        /* from_surface */ true);
    while (!got_snapshot_)
      base::RunLoop().RunUntilIdle();
    SkBitmap bitmap = SkBitmapOperations::CreateTiledBitmap(
        *snapshot_.ToSkBitmap(), /* src_x */ 0, /* src_y */ 0, screenshot_width,
        screenshot_height);

    base::FilePath dir_test_data;
    ASSERT_TRUE(base::PathService::Get(DIR_TEST_DATA, &dir_test_data));
    std::string filename_with_extension = expected_filename;
#if defined(OS_ANDROID)
    filename_with_extension += "_android";
#endif
    filename_with_extension += ".png";
    base::FilePath expected_path =
        dir_test_data.AppendASCII("forms").AppendASCII(filename_with_extension);
    SkBitmap expected_bitmap;
    ASSERT_TRUE(cc::ReadPNGFile(expected_path, &expected_bitmap));

    EXPECT_TRUE(cc::MatchesBitmap(
        bitmap, expected_bitmap,
#if defined(OS_MACOSX)
        // The Mac 10.12 trybot has more significant subpixel rendering
        // differences which we accommodate for here with a large avg/max
        // per-pixel error limit.
        // TODO(crbug.com/1037971): Remove this special case for mac once this
        // bug is resolved.
        cc::FuzzyPixelComparator(/* discard_alpha */ true,
                                 /* error_pixels_percentage_limit */ 7.f,
                                 /* small_error_pixels_percentage_limit */ 0.f,
                                 /* avg_abs_error_limit */ 16.f,
                                 /* max_abs_error_limit */ 79.f,
                                 /* small_error_threshold */ 0)));
#else
        // We use a fuzzy comparator to accommodate for slight
        // differences between the kitkat and marshmallow trybots that aren't
        // visible to the human eye. We use a very low error limit because the
        // pixels that are different are very similar shades of color.
        cc::FuzzyPixelComparator(/* discard_alpha */ true,
                                 /* error_pixels_percentage_limit */ 6.f,
                                 /* small_error_pixels_percentage_limit */ 0.f,
                                 /* avg_abs_error_limit */ 4.f,
                                 /* max_abs_error_limit */ 4.f,
                                 /* small_error_threshold */ 0)));
#endif
  }

  bool got_snapshot_ = false;
  gfx::Image snapshot_;
  std::unique_ptr<base::test::ScopedFeatureList> feature_list_;
};

IN_PROC_BROWSER_TEST_F(FormControlsBrowserTest, Checkbox) {
  RunFormControlsTest(
      "form_controls_browsertest_checkbox",
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
  RunFormControlsTest(
      "form_controls_browsertest_radio",
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
