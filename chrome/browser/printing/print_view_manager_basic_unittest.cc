// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_view_manager_basic.h"

#include "chrome/browser/printing/printing_init.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"

namespace printing {

class PrintViewManagerBasicTest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    InitializePrinting(web_contents());
    content::RenderFrameHostTester::For(main_rfh())
        ->InitializeRenderFrameIfNeeded();
  }
};

TEST_F(PrintViewManagerBasicTest, PrintSubFrameAndDestroy) {
  auto* sub_frame =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("child");
  ASSERT_TRUE(sub_frame);

  auto* print_view_manager =
      PrintViewManagerBasic::FromWebContents(web_contents());
  ASSERT_TRUE(print_view_manager);
  EXPECT_FALSE(print_view_manager->GetPrintingRFHForTesting());

  print_view_manager->PrintNow(sub_frame);
  EXPECT_TRUE(print_view_manager->GetPrintingRFHForTesting());

  content::RenderFrameHostTester::For(sub_frame)->Detach();
  EXPECT_FALSE(print_view_manager->GetPrintingRFHForTesting());
}

}  // namespace printing
