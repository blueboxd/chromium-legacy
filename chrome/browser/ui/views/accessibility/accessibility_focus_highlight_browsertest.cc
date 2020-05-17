// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "math.h"

#include "chrome/browser/ui/views/accessibility/accessibility_focus_highlight.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/snapshot/snapshot.h"
#include "ui/views/widget/widget.h"

class AccessibilityFocusHighlightBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // This is required for the output to be rendered, then captured.
    command_line->AppendSwitch(switches::kEnablePixelOutputInTests);
  }

  AccessibilityFocusHighlightBrowserTest() = default;
  ~AccessibilityFocusHighlightBrowserTest() override = default;
  AccessibilityFocusHighlightBrowserTest(
      const AccessibilityFocusHighlightBrowserTest&) = delete;
  AccessibilityFocusHighlightBrowserTest& operator=(
      const AccessibilityFocusHighlightBrowserTest&) = delete;

  // InProcessBrowserTest overrides:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kAccessibilityFocusHighlight);
    InProcessBrowserTest::SetUp();
  }

  bool ColorsApproximatelyEqual(SkColor color1, SkColor color2) {
    return abs(int{SkColorGetR(color1)} - int{SkColorGetR(color2)}) < 50 &&
           abs(int{SkColorGetG(color1)} - int{SkColorGetG(color2)}) < 50 &&
           abs(int{SkColorGetB(color1)} - int{SkColorGetB(color2)}) < 50;
  }

  float CountPercentPixelsWithColor(const gfx::Image& image, SkColor color) {
    SkBitmap bitmap = image.AsBitmap();
    int count = 0;
    for (int x = 0; x < bitmap.width(); ++x) {
      for (int y = 0; y < bitmap.height(); ++y) {
        if (ColorsApproximatelyEqual(color, bitmap.getColor(x, y)))
          count++;
      }
    }
    return count * 100.0f / (bitmap.width() * bitmap.height());
  }

  gfx::Image CaptureWindowContents() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    views::Widget* widget = browser_view->GetWidget();
    gfx::Rect bounds = widget->GetWindowBoundsInScreen();
    bounds.Offset(-bounds.OffsetFromOrigin());
    gfx::NativeView native_view = widget->GetNativeView();

    // Keep trying until we get a successful capture.
    while (true) {
      // First try sync. If that fails, try async.
      gfx::Image result_image;
      if (!ui::GrabViewSnapshot(native_view, bounds, &result_image)) {
        const auto on_got_snapshot = [](base::RunLoop* run_loop,
                                        gfx::Image* image,
                                        gfx::Image got_image) {
          *image = got_image;
          run_loop->Quit();
        };
        base::RunLoop run_loop;
        ui::GrabViewSnapshotAsync(
            native_view, bounds,
            base::BindOnce(on_got_snapshot, &run_loop, &result_image));
        run_loop.Run();
      }

      if (result_image.Size().IsEmpty()) {
        LOG(INFO) << "Bitmap not correct size, trying to capture again";
        continue;
      }

      // Skip through every 16th pixel (just for speed, no need to check
      // every single one). If we find at least one opaque pixel then we
      // assume we got a valid image. If the capture fails we sometimes get
      // an all transparent image, but when it succeeds there can be a
      // transparent edge.
      bool found_opaque_pixel = false;
      for (int x = 0; x < bounds.width() && !found_opaque_pixel; x += 16) {
        for (int y = 0; y < bounds.height() && !found_opaque_pixel; y += 16) {
          if (SkColorGetA(result_image.AsBitmap().getColor(x, y)) ==
              SK_AlphaOPAQUE) {
            found_opaque_pixel = true;
          }
        }
      }
      if (!found_opaque_pixel) {
        LOG(INFO) << "Bitmap not opaque, trying to capture again";
        continue;
      }

      return result_image;
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Smoke test that ensures that when a node gets focus, the layer with the
// focus highlight actually gets drawn.
IN_PROC_BROWSER_TEST_F(AccessibilityFocusHighlightBrowserTest, DrawsHighlight) {
  ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html,"
                      "<body style='background-color: rgb(204, 255, 255)'>"
                      "<input id='textfield' style='width: 100%'>"));

  AccessibilityFocusHighlight::SetNoFadeForTesting();
  AccessibilityFocusHighlight::SkipActivationCheckForTesting();

  // The web page has a background with a specific color. Keep looping until we
  // capture an image of the page that's more than 90% that color.
  gfx::Image image;
  do {
    base::RunLoop().RunUntilIdle();
    image = CaptureWindowContents();
  } while (CountPercentPixelsWithColor(image, SkColorSetRGB(204, 255, 255)) <
           90.0f);

  // Initially less than 0.01% of the image should be the focus ring's highlight
  // color.
  SkColor highlight_color =
      AccessibilityFocusHighlight::GetHighlightColorForTesting();
  ASSERT_LT(CountPercentPixelsWithColor(image, highlight_color), 0.01f);

  // Focus something.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  std::string script("document.getElementById('textfield').focus();");
  EXPECT_TRUE(content::ExecuteScript(web_contents, script));

  // Now wait until at least 0.1% of the image has the focus ring's highlight
  // color. If it never does, the test will time out.
  do {
    base::RunLoop().RunUntilIdle();
    image = CaptureWindowContents();
  } while (CountPercentPixelsWithColor(image, highlight_color) < 0.1f);
}
