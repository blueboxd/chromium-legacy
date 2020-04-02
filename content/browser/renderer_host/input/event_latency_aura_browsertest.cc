// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/shell/browser/shell.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/test/event_generator.h"

namespace content {

class EventLatencyBrowserTest : public ContentBrowserTest {
 public:
  EventLatencyBrowserTest() = default;
  ~EventLatencyBrowserTest() override = default;

  EventLatencyBrowserTest(const EventLatencyBrowserTest&) = delete;
  EventLatencyBrowserTest& operator=(const EventLatencyBrowserTest&) = delete;

 protected:
  RenderWidgetHostImpl* GetWidgetHost() {
    return RenderWidgetHostImpl::From(
        shell()->web_contents()->GetRenderViewHost()->GetWidget());
  }

  // Starts the test server and navigates to the test page. Returns after the
  // navigation is complete.
  void LoadTestPage() {
    ASSERT_TRUE(embedded_test_server()->Start());

    // Navigate to the test page which has a rAF animation and a main thread
    // animation running.
    GURL test_url =
        embedded_test_server()->GetURL("/event-latency-animation.html");
    EXPECT_TRUE(NavigateToURL(shell(), test_url));

    aura::Window* content = shell()->web_contents()->GetContentNativeView();
    content->GetHost()->SetBoundsInPixels(gfx::Rect(800, 600));

    RenderWidgetHostImpl* host = GetWidgetHost();
    HitTestRegionObserver observer(host->GetFrameSinkId());

    // Wait for the hit test data to be ready.
    observer.WaitForHitTestData();
  }

  void FocusButton() const { ASSERT_TRUE(ExecJs(shell(), "focusButton()")); }

  void StartAnimations() const {
    ASSERT_TRUE(ExecJs(shell(), "startAnimations()"));
  }
};

// Tests that if a key-press on a page causes a visual update, appropriate event
// latency metrics are reported.
IN_PROC_BROWSER_TEST_F(EventLatencyBrowserTest, KeyPressOnButton) {
  base::HistogramTester histogram_tester;

  ASSERT_NO_FATAL_FAILURE(LoadTestPage());
  FocusButton();

  ui::test::EventGenerator generator(shell()
                                         ->web_contents()
                                         ->GetRenderWidgetHostView()
                                         ->GetNativeView()
                                         ->GetRootWindow());

  // Press and release the space key. Since the button on the test page is
  // focused, this should change the visuals of the button and generate a
  // compositor frame with appropriate event latency metrics.
  generator.PressKey(ui::VKEY_SPACE, 0);
  generator.ReleaseKey(ui::VKEY_SPACE, 0);
  RunUntilInputProcessed(GetWidgetHost());

  FetchHistogramsFromChildProcesses();

  base::HistogramTester::CountsMap expected_counts = {
      {"EventLatency.KeyReleased.TotalLatency", 1},
  };
  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix("EventLatency."),
              testing::ContainerEq(expected_counts));
}

// Tests that if a key-press on a page with an animation causes a visual update,
// appropriate event latency metrics are reported.
IN_PROC_BROWSER_TEST_F(EventLatencyBrowserTest, KeyPressOnButtonWithAnimation) {
  base::HistogramTester histogram_tester;

  ASSERT_NO_FATAL_FAILURE(LoadTestPage());
  StartAnimations();
  FocusButton();

  ui::test::EventGenerator generator(shell()
                                         ->web_contents()
                                         ->GetRenderWidgetHostView()
                                         ->GetNativeView()
                                         ->GetRootWindow());

  // Press and release the space key. Since the button on the test page is
  // focused, this should change the visuals of the button and generate a
  // compositor frame with appropriate event latency metrics.
  generator.PressKey(ui::VKEY_SPACE, 0);
  generator.ReleaseKey(ui::VKEY_SPACE, 0);
  RunUntilInputProcessed(GetWidgetHost());

  FetchHistogramsFromChildProcesses();

  base::HistogramTester::CountsMap expected_counts = {
      {"EventLatency.KeyReleased.TotalLatency", 1},
  };
  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix("EventLatency."),
              testing::ContainerEq(expected_counts));
}

}  // namespace content
