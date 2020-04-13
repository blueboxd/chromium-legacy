// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/screenshot_test_utils.h"

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_utils.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/gl/gl_switches.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace content {

void SetUpCommandLineForScreenshotTest(base::CommandLine* command_line) {
  // The --force-device-scale-factor flag helps make the pixel output of
  // different android trybots more similar.
  command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "1.0");

  // The --disable-lcd-text flag helps text render more similarly on
  // different bots and platform.
  command_line->AppendSwitch(switches::kDisableLCDText);
}

void RunScreenshotTest(WebContents* web_contents,
                       const base::FilePath& golden_filepath_default,
                       int screenshot_width,
                       int screenshot_height) {
  // Asserts for SetUpCommandLineForScreenshotTest.
  ASSERT_EQ(base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                switches::kForceDeviceScaleFactor),
            "1.0");
  ASSERT_TRUE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableLCDText));

  // Asserts that BrowserTestBase::EnablePixelOutput was called.
  ASSERT_FALSE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableGLDrawingForTests));

  base::ScopedAllowBlockingForTesting allow_blocking;

  content::RenderWidgetHostImpl* const rwh =
      content::RenderWidgetHostImpl::From(
          web_contents->GetRenderWidgetHostView()->GetRenderWidgetHost());
  ASSERT_TRUE(rwh);

  gfx::Image snapshot;
  base::RunLoop screenshot_callback_runloop;
  base::RepeatingClosure quit_closure =
      screenshot_callback_runloop.QuitClosure();
  rwh->GetSnapshotFromBrowser(
      base::BindLambdaForTesting([&](const gfx::Image& image) {
        snapshot = image;
        quit_closure.Run();
      }),
      /* from_surface */ true);
  screenshot_callback_runloop.Run();

  SkBitmap bitmap = SkBitmapOperations::CreateTiledBitmap(
      *snapshot.ToSkBitmap(), /* src_x */ 0, /* src_y */ 0, screenshot_width,
      screenshot_height);

  base::StringPiece platform_suffix;
#if defined(OS_MACOSX)
  platform_suffix = "_mac";
#elif defined(OS_WIN)
  platform_suffix = "_win";
#elif defined(OS_CHROMEOS)
  platform_suffix = "_chromeos";
#elif defined(OS_ANDROID)
  int sdk_int = base::android::BuildInfo::GetInstance()->sdk_int();
  if (sdk_int == base::android::SDK_VERSION_KITKAT) {
    platform_suffix = "_android_kitkat";
  } else if (sdk_int == base::android::SDK_VERSION_MARSHMALLOW) {
    platform_suffix = "_android_marshmallow";
  } else {
    platform_suffix = "_android";
  }
#endif
  base::FilePath golden_filepath_platform =
      golden_filepath_default.InsertBeforeExtensionASCII(platform_suffix);

  SkBitmap expected_bitmap;
  if (!cc::ReadPNGFile(golden_filepath_platform, &expected_bitmap)) {
    ASSERT_TRUE(cc::ReadPNGFile(golden_filepath_default, &expected_bitmap));
  }

#if defined(OS_MACOSX)
  // The Mac 10.12 trybot has more significant subpixel rendering
  // differences which we accommodate for here with a large avg/max
  // per-pixel error limit.
  // TODO(crbug.com/1037971): Remove this special case for mac once this
  // bug is resolved.
  cc::FuzzyPixelComparator comparator(
      /* discard_alpha */ true,
      /* error_pixels_percentage_limit */ 7.f,
      /* small_error_pixels_percentage_limit */ 0.f,
      /* avg_abs_error_limit */ 16.f,
      /* max_abs_error_limit */ 79.f,
      /* small_error_threshold */ 0);
#else
  cc::ExactPixelComparator comparator(/* disard_alpha */ true);
#endif
  EXPECT_TRUE(cc::MatchesBitmap(bitmap, expected_bitmap, comparator));
}

}  // namespace content
