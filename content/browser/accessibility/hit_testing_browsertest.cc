// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "content/browser/accessibility/accessibility_content_browsertest.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/platform/ax_platform_node_base.h"
#include "ui/display/display_switches.h"

namespace content {

class AccessibilityHitTestingBrowserTest
    : public AccessibilityContentBrowserTest {
 public:
  AccessibilityHitTestingBrowserTest() = default;
  ~AccessibilityHitTestingBrowserTest() override = default;

  BrowserAccessibilityManager* GetRootBrowserAccessibilityManager() {
    WebContentsImpl* web_contents =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    return web_contents->GetRootBrowserAccessibilityManager();
  }

  float GetDeviceScaleFactor() {
    return GetRootBrowserAccessibilityManager()->device_scale_factor();
  }

  gfx::Rect GetViewBoundsInScreenCoordinates() {
    return GetRootBrowserAccessibilityManager()
        ->GetViewBoundsInScreenCoordinates();
  }

  // http://www.chromium.org/developers/design-documents/blink-coordinate-spaces
  // If UseZoomForDSF is enabled, device scale factor gets applied going from
  // CSS to page pixels, i.e. before view offset.
  // if UseZoomForDSF is disabled, device scale factor gets applied going from
  // screen to physical pixels, i.e. after view offset.
  gfx::Point CSSToPagePoint(gfx::Point css_point) {
    gfx::Point page_point;
    if (IsUseZoomForDSFEnabled())
      page_point = ScaleToRoundedPoint(css_point, GetDeviceScaleFactor());
    else
      page_point = css_point;

    return page_point;
  }

  gfx::Point CSSToPhysicalPixelPoint(gfx::Point css_point) {
    gfx::Point page_point = CSSToPagePoint(css_point);

    gfx::Rect screen_view_bounds = GetViewBoundsInScreenCoordinates();
    gfx::Point screen_point =
        page_point + screen_view_bounds.OffsetFromOrigin();

    gfx::Point physical_pixel_point;
    if (IsUseZoomForDSFEnabled()) {
      physical_pixel_point = screen_point;
    } else {
      physical_pixel_point =
          ScaleToRoundedPoint(screen_point, GetDeviceScaleFactor());
    }

    return physical_pixel_point;
  }

  BrowserAccessibility* HitTestAndWaitForResultWithEvent(
      const gfx::Point& point,
      ax::mojom::Event event_to_fire) {
    BrowserAccessibilityManager* manager = GetRootBrowserAccessibilityManager();

    AccessibilityNotificationWaiter event_waiter(
        shell()->web_contents(), ui::kAXModeComplete, event_to_fire);
    ui::AXActionData action_data;
    action_data.action = ax::mojom::Action::kHitTest;
    action_data.target_point = CSSToPagePoint(point);
    action_data.hit_test_event_to_fire = event_to_fire;
    manager->delegate()->AccessibilityPerformAction(action_data);
    event_waiter.WaitForNotification();

    RenderFrameHostImpl* target_frame = event_waiter.event_render_frame_host();
    BrowserAccessibilityManager* target_manager =
        target_frame->browser_accessibility_manager();
    int event_target_id = event_waiter.event_target_id();
    BrowserAccessibility* hit_node = target_manager->GetFromID(event_target_id);
    return hit_node;
  }

  BrowserAccessibility* HitTestAndWaitForResult(const gfx::Point& point) {
    return HitTestAndWaitForResultWithEvent(point, ax::mojom::Event::kHover);
  }

  BrowserAccessibility* TapAndWaitForResult(const gfx::Point& point) {
    AccessibilityNotificationWaiter event_waiter(shell()->web_contents(),
                                                 ui::kAXModeComplete,
                                                 ax::mojom::Event::kClicked);

    SimulateTapAt(shell()->web_contents(), point);
    event_waiter.WaitForNotification();

    RenderFrameHostImpl* target_frame = event_waiter.event_render_frame_host();
    BrowserAccessibilityManager* target_manager =
        target_frame->browser_accessibility_manager();
    int event_target_id = event_waiter.event_target_id();
    BrowserAccessibility* hit_node = target_manager->GetFromID(event_target_id);
    return hit_node;
  }

  BrowserAccessibility* CallCachingAsyncHitTest(const gfx::Point& page_point) {
    gfx::Point screen_point = CSSToPhysicalPixelPoint(page_point);

    // Each call to CachingAsyncHitTest results in at least one HOVER
    // event received. Block until we receive it. CachingAsyncHitTestNearestLeaf
    // will call CachingAsyncHitTest.
    AccessibilityNotificationWaiter hover_waiter(
        shell()->web_contents(), ui::kAXModeComplete, ax::mojom::Event::kHover);

    BrowserAccessibility* result =
        GetRootBrowserAccessibilityManager()->CachingAsyncHitTest(screen_point);

    hover_waiter.WaitForNotification();
    return result;
  }

  ui::AXPlatformNodeBase* CallNearestLeafNode(const gfx::Point& page_point) {
    gfx::Point screen_point = CSSToPhysicalPixelPoint(page_point);
    BrowserAccessibilityManager* manager =
        static_cast<WebContentsImpl*>(shell()->web_contents())
            ->GetRootBrowserAccessibilityManager();

    // Each call to CachingAsyncHitTest results in at least one HOVER
    // event received. Block until we receive it. CachingAsyncHitTest
    // will call CachingAsyncHitTest.
    AccessibilityNotificationWaiter hover_waiter(
        shell()->web_contents(), ui::kAXModeComplete, ax::mojom::Event::kHover);
    ui::AXPlatformNodeBase* result = nullptr;
    if (manager->GetRoot()->GetAXPlatformNode()) {
      result = static_cast<ui::AXPlatformNodeBase*>(
                   manager->GetRoot()->GetAXPlatformNode())
                   ->NearestLeafToPoint(screen_point);
    }
    hover_waiter.WaitForNotification();
    return result;
  }

  RenderWidgetHostImpl* GetRenderWidgetHost() {
    return RenderWidgetHostImpl::From(shell()
                                          ->web_contents()
                                          ->GetRenderWidgetHostView()
                                          ->GetRenderWidgetHost());
  }

  void SynchronizeThreads() {
    MainThreadFrameObserver observer(GetRenderWidgetHost());
    observer.Wait();
  }
};

class AccessibilityHitTestingCrossProcessBrowserTest
    : public AccessibilityHitTestingBrowserTest {
 public:
  AccessibilityHitTestingCrossProcessBrowserTest() {}
  ~AccessibilityHitTestingCrossProcessBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

using AccessibilityZoomTestParam = std::tuple<double, bool>;

class AccessibilityHitTestingZoomBrowserTest
    : public AccessibilityHitTestingBrowserTest,
      public ::testing::WithParamInterface<AccessibilityZoomTestParam> {
 public:
  AccessibilityHitTestingZoomBrowserTest() = default;
  ~AccessibilityHitTestingZoomBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    double device_scale_factor;
    bool use_zoom_for_dsf;
    std::tie(device_scale_factor, use_zoom_for_dsf) = GetParam();
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kForceDeviceScaleFactor,
        base::StringPrintf("%.2f", device_scale_factor));
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kEnableUseZoomForDSF, use_zoom_for_dsf ? "true" : "false");
  }

  struct TestPassToString {
    std::string operator()(
        const ::testing::TestParamInfo<AccessibilityZoomTestParam>& info)
        const {
      double device_scale_factor;
      bool use_zoom_for_dsf;
      std::tie(device_scale_factor, use_zoom_for_dsf) = info.param;
      return base::StringPrintf("ZoomFactor%g_UseZoomForDSF%s",
                                device_scale_factor,
                                use_zoom_for_dsf ? "On" : "Off");
    }
  };
};

INSTANTIATE_TEST_SUITE_P(
    All,
    AccessibilityHitTestingZoomBrowserTest,
    ::testing::Combine(::testing::Values(1, 2), ::testing::Bool()),
    AccessibilityHitTestingZoomBrowserTest::TestPassToString());

IN_PROC_BROWSER_TEST_P(AccessibilityHitTestingZoomBrowserTest,
                       CachingAsyncHitTest) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  GURL url(embedded_test_server()->GetURL(
      "/accessibility/hit_testing/simple_rectangles.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  waiter.WaitForNotification();

  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "rectA");

  // Test a hit on a rect in the main frame.
  gfx::Point rect2_point(49, 20);
  BrowserAccessibility* hit_node = CallCachingAsyncHitTest(rect2_point);
  BrowserAccessibility* expected_node =
      FindNode(ax::mojom::Role::kGenericContainer, "rect2");

  // Compare several properties so that we generate rich log output if the test
  // fails.
  EXPECT_EQ(expected_node->GetName(), hit_node->GetName());
  EXPECT_EQ(expected_node->GetId(), hit_node->GetId());
  EXPECT_EQ(expected_node->GetClippedScreenBoundsRect(),
            hit_node->GetClippedScreenBoundsRect());

  // Test a hit on a rect in the iframe.
  gfx::Point rectB_point(79, 79);
  hit_node = CallCachingAsyncHitTest(rectB_point);
  expected_node = FindNode(ax::mojom::Role::kGenericContainer, "rectB");

  // Compare several properties so that we generate rich log output if the test
  // fails.
  EXPECT_EQ(expected_node->GetName(), hit_node->GetName());
  EXPECT_EQ(expected_node->GetId(), hit_node->GetId());
  EXPECT_EQ(expected_node->GetClippedScreenBoundsRect(),
            hit_node->GetClippedScreenBoundsRect());
}

IN_PROC_BROWSER_TEST_P(AccessibilityHitTestingZoomBrowserTest, HitTest) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  GURL url(embedded_test_server()->GetURL(
      "/accessibility/hit_testing/simple_rectangles.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  waiter.WaitForNotification();

  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "rectA");

  // Test a hit on a rect in the main frame.
  gfx::Point rect2_point(49, 20);
  BrowserAccessibility* hit_node = HitTestAndWaitForResult(rect2_point);
  BrowserAccessibility* expected_node =
      FindNode(ax::mojom::Role::kGenericContainer, "rect2");

  // Compare several properties so that we generate rich log output if the test
  // fails.
  EXPECT_EQ(expected_node->GetName(), hit_node->GetName());
  EXPECT_EQ(expected_node->GetId(), hit_node->GetId());
  EXPECT_EQ(expected_node->GetClippedScreenBoundsRect(),
            hit_node->GetClippedScreenBoundsRect());

  // Test a hit on a rect in the iframe.
  gfx::Point rectB_point(79, 79);
  hit_node = HitTestAndWaitForResult(rectB_point);
  expected_node = FindNode(ax::mojom::Role::kGenericContainer, "rectB");

  // Compare several properties so that we generate rich log output if the test
  // fails.
  EXPECT_EQ(expected_node->GetName(), hit_node->GetName());
  EXPECT_EQ(expected_node->GetId(), hit_node->GetId());
  EXPECT_EQ(expected_node->GetClippedScreenBoundsRect(),
            hit_node->GetClippedScreenBoundsRect());
}

IN_PROC_BROWSER_TEST_F(AccessibilityHitTestingBrowserTest,
                       HitTestOutsideDocumentBoundsReturnsRoot) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  // Load the page.
  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  const char url_str[] =
      "data:text/html,"
      "<!doctype html>"
      "<html><head><title>Accessibility Test</title></head>"
      "<body>"
      "<a href='#'>"
      "This is some text in a link"
      "</a>"
      "</body></html>";
  GURL url(url_str);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  waiter.WaitForNotification();

  BrowserAccessibility* hit_node = HitTestAndWaitForResult(gfx::Point(-1, -1));
  ASSERT_TRUE(hit_node != nullptr);
  ASSERT_EQ(ax::mojom::Role::kRootWebArea, hit_node->GetRole());
}

IN_PROC_BROWSER_TEST_F(AccessibilityHitTestingBrowserTest,
                       HitTestingInIframes) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  GURL url(embedded_test_server()->GetURL(
      "/accessibility/html/iframe-coordinates.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  waiter.WaitForNotification();

  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "Ordinary Button");
  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "Scrolled Button");

  // Send a series of hit test requests, and for each one
  // wait for the hover event in response, verifying we hit the
  // correct object.

  // (26, 26) -> "Button"
  BrowserAccessibility* hit_node;
  hit_node = HitTestAndWaitForResult(gfx::Point(26, 26));
  ASSERT_TRUE(hit_node != nullptr);
  ASSERT_EQ(ax::mojom::Role::kButton, hit_node->GetRole());
  ASSERT_EQ("Button",
            hit_node->GetStringAttribute(ax::mojom::StringAttribute::kName));

  // (50, 50) -> "Button"
  hit_node = HitTestAndWaitForResult(gfx::Point(50, 50));
  ASSERT_TRUE(hit_node != nullptr);
  ASSERT_EQ(ax::mojom::Role::kButton, hit_node->GetRole());
  ASSERT_EQ("Button",
            hit_node->GetStringAttribute(ax::mojom::StringAttribute::kName));

  // (50, 305) -> div in first iframe
  hit_node = HitTestAndWaitForResult(gfx::Point(50, 305));
  ASSERT_TRUE(hit_node != nullptr);
  ASSERT_EQ(ax::mojom::Role::kGenericContainer, hit_node->GetRole());

  // (50, 350) -> "Ordinary Button"
  hit_node = HitTestAndWaitForResult(gfx::Point(50, 350));
  ASSERT_TRUE(hit_node != nullptr);
  ASSERT_EQ(ax::mojom::Role::kButton, hit_node->GetRole());
  ASSERT_EQ("Ordinary Button",
            hit_node->GetStringAttribute(ax::mojom::StringAttribute::kName));

  // (50, 455) -> "Scrolled Button"
  hit_node = HitTestAndWaitForResult(gfx::Point(50, 455));
  ASSERT_TRUE(hit_node != nullptr);
  ASSERT_EQ(ax::mojom::Role::kButton, hit_node->GetRole());
  ASSERT_EQ("Scrolled Button",
            hit_node->GetStringAttribute(ax::mojom::StringAttribute::kName));

  // (50, 505) -> div in second iframe
  hit_node = HitTestAndWaitForResult(gfx::Point(50, 505));
  ASSERT_TRUE(hit_node != nullptr);
  ASSERT_EQ(ax::mojom::Role::kGenericContainer, hit_node->GetRole());

  // (50, 505) -> div in second iframe
  // but with a different event
  hit_node = HitTestAndWaitForResultWithEvent(gfx::Point(50, 505),
                                              ax::mojom::Event::kAlert);
  ASSERT_NE(hit_node, nullptr);
  ASSERT_EQ(ax::mojom::Role::kGenericContainer, hit_node->GetRole());
}

IN_PROC_BROWSER_TEST_F(AccessibilityHitTestingCrossProcessBrowserTest,
                       HitTestingInCrossProcessIframes) {
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/accessibility/hit_testing/hit_testing_a.html"));
  GURL url_b(embedded_test_server()->GetURL(
      "b.com", "/accessibility/hit_testing/hit_testing_b.html"));
  GURL url_c(embedded_test_server()->GetURL(
      "c.com", "/accessibility/hit_testing/hit_testing_c.html"));

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));
  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);

  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  waiter.WaitForNotification();
  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "Button A");

  auto* web_contents = static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = web_contents->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child = root->child_at(0);
  NavigateFrameToURL(child, url_b);
  EXPECT_EQ(url_b, child->current_url());
  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "Button B");
  ASSERT_EQ(1U, child->child_count());

  FrameTreeNode* grand_child = child->child_at(0);
  NavigateFrameToURL(grand_child, url_c);
  EXPECT_EQ(url_c, grand_child->current_url());
  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "Button C");

  FrameTreeVisualizer visualizer;
  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   +--Site B ------- proxies for A C\n"
      "        +--Site C -- proxies for A B\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/\n"
      "      C = http://c.com/",
      visualizer.DepictFrameTree(root));

  {
    // (26, 26) -> "Button A"
    BrowserAccessibility* hit_node;
    hit_node = HitTestAndWaitForResult(gfx::Point(26, 26));
    ASSERT_TRUE(hit_node != nullptr);
    ASSERT_EQ(ax::mojom::Role::kButton, hit_node->GetRole());
    ASSERT_EQ("Button A",
              hit_node->GetStringAttribute(ax::mojom::StringAttribute::kName));
  }

  {
    // (26, 176) -> "Button B"
    // 176 = height of div in parent (150), plus button offset (26).
    BrowserAccessibility* hit_node;
    hit_node = HitTestAndWaitForResult(gfx::Point(26, 176));
    ASSERT_TRUE(hit_node != nullptr);
    ASSERT_EQ(ax::mojom::Role::kButton, hit_node->GetRole());
    ASSERT_EQ("Button B",
              hit_node->GetStringAttribute(ax::mojom::StringAttribute::kName));
  }

  {
    // (26, 326) -> "Button C"
    // 326 = 2x height of div in ancestors (300), plus button offset (26).
    BrowserAccessibility* hit_node;
    hit_node = HitTestAndWaitForResult(gfx::Point(26, 326));
    ASSERT_TRUE(hit_node != nullptr);
    ASSERT_EQ(ax::mojom::Role::kButton, hit_node->GetRole());
    ASSERT_EQ("Button C",
              hit_node->GetStringAttribute(ax::mojom::StringAttribute::kName));
  }
}

IN_PROC_BROWSER_TEST_F(AccessibilityHitTestingCrossProcessBrowserTest,
                       HitTestingInScrolledCrossProcessIframe) {
  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/accessibility/hit_testing/hit_testing_a.html"));
  GURL url_b(embedded_test_server()->GetURL(
      "b.com", "/accessibility/hit_testing/hit_testing_b_tall.html"));

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));
  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);

  EXPECT_TRUE(NavigateToURL(shell(), url_a));
  waiter.WaitForNotification();
  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "Button A");

  auto* web_contents = static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = web_contents->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child = root->child_at(0);
  NavigateFrameToURL(child, url_b);
  EXPECT_EQ(url_b, child->current_url());
  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "Button B");
  ASSERT_EQ(1U, child->child_count());

  // Before scrolling.
  {
    // (26, 476) -> "Button B"
    // 476 = height of div in parent (150), plus the placeholder div height
    // (300), plus button offset (26).
    BrowserAccessibility* hit_node;
    hit_node = HitTestAndWaitForResult(gfx::Point(26, 476));
    ASSERT_TRUE(hit_node != nullptr);
    ASSERT_EQ(ax::mojom::Role::kButton, hit_node->GetRole());
    ASSERT_EQ("Button B",
              hit_node->GetStringAttribute(ax::mojom::StringAttribute::kName));
  }

  // Scroll div up 100px.
  int scroll_delta = 100;
  double actual_scroll_delta = 0;
  std::string scroll_string = base::StringPrintf(
      "window.scrollTo(0, %d); "
      "window.domAutomationController.send(window.scrollY);",
      scroll_delta);
  EXPECT_TRUE(ExecuteScriptAndExtractDouble(
      child->current_frame_host(), scroll_string, &actual_scroll_delta));
  EXPECT_NEAR(static_cast<double>(scroll_delta), actual_scroll_delta, 1.0);

  // After scrolling.
  {
    // (26, 376) -> "Button B"
    // 376 = height of div in parent (150), plus the placeholder div height
    // (300), plus button offset (26), less the scroll delta.
    BrowserAccessibility* hit_node;
    hit_node = HitTestAndWaitForResult(gfx::Point(26, 476 - scroll_delta));
    ASSERT_TRUE(hit_node != nullptr);
    ASSERT_EQ(ax::mojom::Role::kButton, hit_node->GetRole());
    ASSERT_EQ("Button B",
              hit_node->GetStringAttribute(ax::mojom::StringAttribute::kName));
  }
}

IN_PROC_BROWSER_TEST_F(AccessibilityHitTestingBrowserTest,
                       CachingAsyncHitTestingInIframes) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  GURL url(embedded_test_server()->GetURL(
      "/accessibility/hit_testing/hit_testing.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  waiter.WaitForNotification();

  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "Ordinary Button");
  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "Scrolled Button");

  // For each point we try, the first time we call CachingAsyncHitTest it
  // should FAIL and return the wrong object, because this test page has
  // been designed to confound local synchronous hit testing using
  // z-indexes. However, calling CachingAsyncHitTest a second time should
  // return the correct result (since CallCachingAsyncHitTest waits for the
  // HOVER event to be received).

  // (50, 50) -> "Button"
  BrowserAccessibility* hit_node;
  hit_node = CallCachingAsyncHitTest(gfx::Point(50, 50));
  ASSERT_TRUE(hit_node != nullptr);
  EXPECT_NE(ax::mojom::Role::kButton, hit_node->GetRole());
  hit_node = CallCachingAsyncHitTest(gfx::Point(50, 50));
  EXPECT_EQ("Button",
            hit_node->GetStringAttribute(ax::mojom::StringAttribute::kName));

  // (50, 305) -> div in first iframe
  hit_node = CallCachingAsyncHitTest(gfx::Point(50, 305));
  ASSERT_TRUE(hit_node != nullptr);
  EXPECT_NE(ax::mojom::Role::kGenericContainer, hit_node->GetRole());
  hit_node = CallCachingAsyncHitTest(gfx::Point(50, 305));
  EXPECT_EQ(ax::mojom::Role::kGenericContainer, hit_node->GetRole());

  // (50, 350) -> "Ordinary Button"
  hit_node = CallCachingAsyncHitTest(gfx::Point(50, 350));
  ASSERT_TRUE(hit_node != nullptr);
  EXPECT_NE(ax::mojom::Role::kButton, hit_node->GetRole());
  hit_node = CallCachingAsyncHitTest(gfx::Point(50, 350));
  EXPECT_EQ(ax::mojom::Role::kButton, hit_node->GetRole());
  EXPECT_EQ("Ordinary Button",
            hit_node->GetStringAttribute(ax::mojom::StringAttribute::kName));

  // (50, 455) -> "Scrolled Button"
  hit_node = CallCachingAsyncHitTest(gfx::Point(50, 455));
  ASSERT_TRUE(hit_node != nullptr);
  EXPECT_NE(ax::mojom::Role::kButton, hit_node->GetRole());
  hit_node = CallCachingAsyncHitTest(gfx::Point(50, 455));
  EXPECT_EQ(ax::mojom::Role::kButton, hit_node->GetRole());
  EXPECT_EQ("Scrolled Button",
            hit_node->GetStringAttribute(ax::mojom::StringAttribute::kName));

  // (50, 505) -> div in second iframe
  hit_node = CallCachingAsyncHitTest(gfx::Point(50, 505));
  ASSERT_TRUE(hit_node != nullptr);
  EXPECT_NE(ax::mojom::Role::kGenericContainer, hit_node->GetRole());
  hit_node = CallCachingAsyncHitTest(gfx::Point(50, 505));
  EXPECT_EQ(ax::mojom::Role::kGenericContainer, hit_node->GetRole());
}

#if !defined(OS_ANDROID) && !defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(AccessibilityHitTestingBrowserTest,
                       HitTestingWithPinchZoom) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);

  const char url_str[] =
      "data:text/html,"
      "<!doctype html>"
      "<html>"
      "<head><title>Accessibility Test</title>"
      "<style>body {margin: 0px;}"
      "button {display: block; height: 50px; width: 50px}</style>"
      "</head>"
      "<body>"
      "<button>Button 1</button>"
      "<button>Button 2</button>"
      "</body></html>";

  GURL url(url_str);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  SynchronizeThreads();
  waiter.WaitForNotification();

  BrowserAccessibility* hit_node;

  // Use a tap event instead of a hittest to make sure that we are using
  // px as input, rather than dips.

  // (10, 10) -> "Button 1"
  hit_node = TapAndWaitForResult(gfx::Point(10, 10));
  ASSERT_TRUE(hit_node != nullptr);
  EXPECT_EQ(ax::mojom::Role::kButton, hit_node->GetRole());
  EXPECT_EQ("Button 1",
            hit_node->GetStringAttribute(ax::mojom::StringAttribute::kName));

  // (60, 60) -> No button there, hits the ignored <body> node
  hit_node = TapAndWaitForResult(gfx::Point(60, 60));
  EXPECT_NE(nullptr, hit_node);
  EXPECT_EQ(ax::mojom::Role::kGenericContainer, hit_node->GetRole());
  EXPECT_TRUE(hit_node->HasState(ax::mojom::State::kIgnored));
  EXPECT_EQ("body",
            hit_node->GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag));

  // (10, 60) -> "Button 2"
  hit_node = TapAndWaitForResult(gfx::Point(10, 60));
  ASSERT_TRUE(hit_node != nullptr);
  EXPECT_EQ(ax::mojom::Role::kButton, hit_node->GetRole());
  EXPECT_EQ("Button 2",
            hit_node->GetStringAttribute(ax::mojom::StringAttribute::kName));

  content::TestPageScaleObserver scale_observer(shell()->web_contents());
  const gfx::Rect contents_rect = shell()->web_contents()->GetContainerBounds();
  const gfx::Point pinch_position(contents_rect.x(), contents_rect.y());
  SimulateGesturePinchSequence(shell()->web_contents(), pinch_position, 2.0f,
                               blink::WebGestureDevice::kTouchscreen);
  scale_observer.WaitForPageScaleUpdate();

  // (10, 10) -> "Button 1"
  hit_node = TapAndWaitForResult(gfx::Point(10, 10));
  ASSERT_TRUE(hit_node != nullptr);
  EXPECT_EQ(ax::mojom::Role::kButton, hit_node->GetRole());
  EXPECT_EQ("Button 1",
            hit_node->GetStringAttribute(ax::mojom::StringAttribute::kName));

  // (60, 60) -> "Button 1"
  hit_node = TapAndWaitForResult(gfx::Point(60, 60));
  ASSERT_TRUE(hit_node != nullptr);
  EXPECT_EQ(ax::mojom::Role::kButton, hit_node->GetRole());
  EXPECT_EQ("Button 1",
            hit_node->GetStringAttribute(ax::mojom::StringAttribute::kName));

  // (10, 60) -> "Button 1"
  hit_node = TapAndWaitForResult(gfx::Point(10, 60));
  ASSERT_TRUE(hit_node != nullptr);
  EXPECT_EQ(ax::mojom::Role::kButton, hit_node->GetRole());
  EXPECT_EQ("Button 1",
            hit_node->GetStringAttribute(ax::mojom::StringAttribute::kName));

  // (10, 110) -> "Button 2"
  hit_node = TapAndWaitForResult(gfx::Point(10, 110));
  ASSERT_TRUE(hit_node != nullptr);
  EXPECT_EQ(ax::mojom::Role::kButton, hit_node->GetRole());
  EXPECT_EQ("Button 2",
            hit_node->GetStringAttribute(ax::mojom::StringAttribute::kName));

  // (190, 190) -> "Button 2"
  hit_node = TapAndWaitForResult(gfx::Point(90, 190));
  ASSERT_TRUE(hit_node != nullptr);
  EXPECT_EQ(ax::mojom::Role::kButton, hit_node->GetRole());
  EXPECT_EQ("Button 2",
            hit_node->GetStringAttribute(ax::mojom::StringAttribute::kName));
}

IN_PROC_BROWSER_TEST_F(AccessibilityHitTestingBrowserTest,
                       HitTestingWithPinchZoomAndIframes) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);

  GURL url(embedded_test_server()->GetURL(
      "/accessibility/html/iframe-coordinates.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  SynchronizeThreads();
  waiter.WaitForNotification();

  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "Ordinary Button");
  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "Scrolled Button");

  content::TestPageScaleObserver scale_observer(shell()->web_contents());
  const gfx::Rect contents_rect = shell()->web_contents()->GetContainerBounds();
  const gfx::Point pinch_position(contents_rect.x(), contents_rect.y());

  SimulateGesturePinchSequence(shell()->web_contents(), pinch_position, 1.25f,
                               blink::WebGestureDevice::kTouchscreen);
  scale_observer.WaitForPageScaleUpdate();

  BrowserAccessibility* hit_node;

  // (26, 26) -> No button because of pinch.
  hit_node = TapAndWaitForResult(gfx::Point(26, 26));
  ASSERT_TRUE(hit_node != nullptr);
  EXPECT_NE(ax::mojom::Role::kButton, hit_node->GetRole());

  // (63, 63) -> "Button"
  hit_node = TapAndWaitForResult(gfx::Point(63, 63));
  ASSERT_TRUE(hit_node != nullptr);
  EXPECT_EQ(ax::mojom::Role::kButton, hit_node->GetRole());
  EXPECT_EQ("Button",
            hit_node->GetStringAttribute(ax::mojom::StringAttribute::kName));

  // (63, 438) -> "Ordinary Button"
  hit_node = TapAndWaitForResult(gfx::Point(63, 438));
  ASSERT_TRUE(hit_node != nullptr);
  EXPECT_EQ(ax::mojom::Role::kButton, hit_node->GetRole());
  EXPECT_EQ("Ordinary Button",
            hit_node->GetStringAttribute(ax::mojom::StringAttribute::kName));

  // (63, 569) -> "Scrolled Button"
  hit_node = TapAndWaitForResult(gfx::Point(63, 569));
  ASSERT_TRUE(hit_node != nullptr);
  EXPECT_EQ(ax::mojom::Role::kButton, hit_node->GetRole());
  EXPECT_EQ("Scrolled Button",
            hit_node->GetStringAttribute(ax::mojom::StringAttribute::kName));
}

#endif  // !defined(OS_ANDROID) && !defined(OS_MACOSX)

// GetAXPlatformNode is currently only supported on windows and linux (excluding
// Chrome OS or Chromecast)
#if defined(OS_WIN) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS) && !BUILDFLAG(IS_CHROMECAST))
IN_PROC_BROWSER_TEST_F(AccessibilityHitTestingBrowserTest,
                       NearestLeafInIframes) {
  ASSERT_TRUE(embedded_test_server()->Start());

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  GURL url(embedded_test_server()->GetURL(
      "/accessibility/hit_testing/hit_testing.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  waiter.WaitForNotification();

  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "Ordinary Button");
  WaitForAccessibilityTreeToContainNodeWithName(shell()->web_contents(),
                                                "Scrolled Button");

  // For each point we try, the first time we call CachingAsyncHitTest it
  // should FAIL and return the wrong object, because this test page has
  // been designed to confound local synchronous hit testing using
  // z-indexes. However, calling CachingAsyncHitTest a second time should
  // return the correct result (since CallCachingAsyncHitTest waits for the
  // HOVER event to be received). CachingAsyncHitTest is called by
  // GetNearestLeaf

  // (50, 50) -> "Button"
  ui::AXPlatformNodeBase* hit_node;
  hit_node = CallNearestLeafNode(gfx::Point(50, 50));
  ASSERT_TRUE(hit_node != nullptr);
  EXPECT_NE(ax::mojom::Role::kButton, hit_node->GetData().role);
  hit_node = CallNearestLeafNode(gfx::Point(50, 50));
  ASSERT_TRUE(hit_node != nullptr);
  EXPECT_EQ("Button",
            hit_node->GetStringAttribute(ax::mojom::StringAttribute::kName));

  // (280, 50) -> "Button" is still the closest node to the cursor.
  hit_node = CallNearestLeafNode(gfx::Point(280, 50));
  EXPECT_NE(ax::mojom::Role::kButton, hit_node->GetData().role);
  hit_node = CallNearestLeafNode(gfx::Point(280, 50));
  EXPECT_EQ("Button",
            hit_node->GetStringAttribute(ax::mojom::StringAttribute::kName));

  // (50, 305) -> "Ordinary Button" is the closest leaf node.
  hit_node = CallNearestLeafNode(gfx::Point(50, 305));
  ASSERT_TRUE(hit_node != nullptr);
  EXPECT_NE(ax::mojom::Role::kButton, hit_node->GetData().role);
  hit_node = CallNearestLeafNode(gfx::Point(50, 305));
  ASSERT_TRUE(hit_node != nullptr);
  EXPECT_EQ(ax::mojom::Role::kButton, hit_node->GetData().role);
  EXPECT_EQ("Ordinary Button",
            hit_node->GetStringAttribute(ax::mojom::StringAttribute::kName));

  // (50, 350) -> "Ordinary Button". As we are still within the previous cached
  // hit test's bounds, the subsequent call correctly gets the descendant.
  hit_node = CallNearestLeafNode(gfx::Point(50, 350));
  ASSERT_TRUE(hit_node != nullptr);
  EXPECT_EQ(ax::mojom::Role::kButton, hit_node->GetData().role);
  EXPECT_EQ("Ordinary Button",
            hit_node->GetStringAttribute(ax::mojom::StringAttribute::kName));

  // (50, 455) -> "Scrolled Button"
  hit_node = CallNearestLeafNode(gfx::Point(50, 455));
  ASSERT_TRUE(hit_node != nullptr);
  EXPECT_NE(ax::mojom::Role::kButton, hit_node->GetData().role);
  hit_node = CallNearestLeafNode(gfx::Point(50, 455));
  ASSERT_TRUE(hit_node != nullptr);
  EXPECT_EQ(ax::mojom::Role::kButton, hit_node->GetData().role);
  EXPECT_EQ("Scrolled Button",
            hit_node->GetStringAttribute(ax::mojom::StringAttribute::kName));

  // (50, 505) -> "Scrolled Button"
  hit_node = CallNearestLeafNode(gfx::Point(50, 505));
  ASSERT_TRUE(hit_node != nullptr);
  EXPECT_NE(ax::mojom::Role::kButton, hit_node->GetData().role);
  hit_node = CallNearestLeafNode(gfx::Point(50, 505));
  ASSERT_TRUE(hit_node != nullptr);
  EXPECT_EQ(ax::mojom::Role::kButton, hit_node->GetData().role);
  EXPECT_EQ("Scrolled Button",
            hit_node->GetStringAttribute(ax::mojom::StringAttribute::kName));
}
#endif
}  // namespace content
