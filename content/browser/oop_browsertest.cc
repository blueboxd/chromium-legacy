// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "gpu/config/gpu_switches.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/gl/gl_switches.h"

namespace content {
namespace {

class OOPBrowserTest : public ContentBrowserTest {
 public:
  void SetUp() override {
    EnablePixelOutput();
    ContentBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableGpuRasterization);
    command_line->AppendSwitch(switches::kEnableOopRasterization);
    command_line->AppendSwitch(switches::kUseGpuInTests);
  }

  SkBitmap snapshot_;
};

// This test calls into system GL which is not instrumented with MSAN.
#if !defined(MEMORY_SANITIZER)
// Failing on linux-trusty-rel bot. https://crbug.com/1067662
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#define MAYBE_Basic DISABLED_Basic
#else
#define MAYBE_Basic Basic
#endif
IN_PROC_BROWSER_TEST_F(OOPBrowserTest, MAYBE_Basic) {
  // Create a div to ensure we don't use solid color quads.
  GURL url = GURL(
      "data:text/html,"
      "<style>div{background-color:blue; width:100; height:100;}</style>"
      "<body bgcolor=blue><div></div></body>");
  NavigateToURLBlockUntilNavigationsComplete(shell(), url, 1);

  // Wait for the renderer to submit a frame.
  RenderFrameSubmissionObserver frame_observer(
      RenderWidgetHostImpl::From(
          shell()->web_contents()->GetRenderViewHost()->GetWidget())
          ->render_frame_metadata_provider());
  frame_observer.WaitForAnyFrameSubmission();

  const gfx::Size kComparisonSize(300, 300);

  base::FilePath reference =
      content::GetTestFilePath(nullptr, "oopbrowsertest-basic.png");
  EXPECT_TRUE(CompareWebContentsOutputToReference(shell()->web_contents(),
                                                  reference, kComparisonSize));
}
#endif

}  // namespace
}  // namespace content
