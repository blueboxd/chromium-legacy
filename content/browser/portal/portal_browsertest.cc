// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/frame_host/render_frame_host_manager.h"
#include "content/browser/frame_host/render_frame_proxy_host.h"
#include "content/browser/portal/portal.h"
#include "content/browser/portal/portal_created_observer.h"
#include "content/browser/portal/portal_interceptor_for_testing.h"
#include "content/browser/renderer_host/input/synthetic_smooth_scroll_gesture.h"
#include "content/browser/renderer_host/input/synthetic_tap_gesture.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "mojo/public/cpp/bindings/strong_associated_binding.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/portal/portal.mojom.h"
#include "url/url_constants.h"

using testing::_;

namespace content {

class PortalBrowserTest : public ContentBrowserTest {
 protected:
  PortalBrowserTest() {}

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(blink::features::kPortals);
    ContentBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kValidateInputEventStream);
    command_line->AppendSwitchASCII("--enable-blink-features",
                                    "OverscrollCustomization");
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ContentBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the renderer can create a Portal.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, CreatePortal) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  PortalCreatedObserver portal_created_observer(main_frame);
  EXPECT_TRUE(
      ExecJs(main_frame,
             "document.body.appendChild(document.createElement('portal'));"));
  Portal* portal = portal_created_observer.WaitUntilPortalCreated();
  EXPECT_NE(nullptr, portal);
}

// Tests the the renderer can navigate a Portal.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, NavigatePortal) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  PortalCreatedObserver portal_created_observer(main_frame);

  // Tests that a portal can navigate by setting its src before appending it to
  // the DOM.
  PortalInterceptorForTesting* portal_interceptor;
  WebContents* portal_contents;
  {
    GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
    TestNavigationObserver navigation_observer(a_url);
    navigation_observer.StartWatchingNewWebContents();
    EXPECT_TRUE(ExecJs(
        main_frame,
        base::StringPrintf("var portal = document.createElement('portal');"
                           "portal.src = '%s';"
                           "document.body.appendChild(portal);",
                           a_url.spec().c_str())));

    portal_interceptor = PortalInterceptorForTesting::From(
        portal_created_observer.WaitUntilPortalCreated());
    portal_contents = portal_interceptor->GetPortalContents();
    EXPECT_NE(nullptr, portal_contents);

    navigation_observer.Wait();
    EXPECT_EQ(portal_contents->GetLastCommittedURL(), a_url);
  }

  // Tests that a portal can navigate by setting its src.
  {
    TestNavigationObserver navigation_observer(portal_contents);

    GURL b_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
    EXPECT_TRUE(ExecJs(
        main_frame,
        base::StringPrintf("document.querySelector('portal').src = '%s';",
                           b_url.spec().c_str())));
    navigation_observer.Wait();
    EXPECT_EQ(navigation_observer.last_navigation_url(), b_url);
    EXPECT_EQ(portal_contents->GetLastCommittedURL(), b_url);
  }

  // Tests that a portal can navigating by attribute.
  {
    TestNavigationObserver navigation_observer(portal_contents);

    GURL c_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
    EXPECT_TRUE(ExecJs(
        main_frame,
        base::StringPrintf(
            "document.querySelector('portal').setAttribute('src', '%s');",
            c_url.spec().c_str())));
    navigation_observer.Wait();
    EXPECT_EQ(navigation_observer.last_navigation_url(), c_url);
    EXPECT_EQ(portal_contents->GetLastCommittedURL(), c_url);
  }
}

// Tests that a portal can be activated.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, ActivatePortal) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  Portal* portal = nullptr;
  {
    PortalCreatedObserver portal_created_observer(main_frame);
    GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
    EXPECT_TRUE(ExecJs(
        main_frame, JsReplace("var portal = document.createElement('portal');"
                              "portal.src = $1;"
                              "document.body.appendChild(portal);",
                              a_url)));
    portal = portal_created_observer.WaitUntilPortalCreated();
  }
  PortalInterceptorForTesting* portal_interceptor =
      PortalInterceptorForTesting::From(portal);

  // Ensure that the portal WebContents exists and is different from the tab's
  // WebContents.
  WebContents* portal_contents = portal->GetPortalContents();
  EXPECT_NE(nullptr, portal_contents);
  EXPECT_NE(portal_contents, shell()->web_contents());

  ExecuteScriptAsync(main_frame,
                     "document.querySelector('portal').activate();");
  portal_interceptor->WaitForActivate();

  // After activation, the shell's WebContents should be the previous portal's
  // WebContents.
  EXPECT_EQ(portal_contents, shell()->web_contents());
}

// Tests if a portal can be activated and the predecessor can be adopted.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, ReactivatePredecessor) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  Portal* portal = nullptr;
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  TestNavigationObserver navigation_observer(a_url);
  navigation_observer.StartWatchingNewWebContents();
  {
    PortalCreatedObserver portal_created_observer(main_frame);
    EXPECT_TRUE(ExecJs(
        main_frame, JsReplace("var portal = document.createElement('portal');"
                              "portal.src = $1;"
                              "document.body.appendChild(portal);",
                              a_url)));
    portal = portal_created_observer.WaitUntilPortalCreated();
  }
  PortalInterceptorForTesting* portal_interceptor =
      PortalInterceptorForTesting::From(portal);

  // Ensure that the portal WebContents exists and is different from the tab's
  // WebContents.
  WebContentsImpl* portal_contents = portal->GetPortalContents();
  EXPECT_NE(nullptr, portal_contents);
  EXPECT_NE(portal_contents, shell()->web_contents());
  navigation_observer.Wait();

  RenderFrameHostImpl* portal_frame = portal_contents->GetMainFrame();
  EXPECT_TRUE(ExecJs(portal_frame,
                     "window.addEventListener('portalactivate', e => { "
                     "  var portal = e.adoptPredecessor(); "
                     "  document.body.appendChild(portal); "
                     "});"));

  {
    PortalCreatedObserver adoption_observer(portal_frame);
    EXPECT_TRUE(ExecJs(main_frame,
                       "portal.activate().then(() => { "
                       "  document.body.removeChild(portal); "
                       "});"));
    portal_interceptor->WaitForActivate();
    adoption_observer.WaitUntilPortalCreated();
  }
  // After activation, the shell's WebContents should be the previous portal's
  // WebContents.
  EXPECT_EQ(portal_contents, shell()->web_contents());
  // The original predecessor WebContents should be adopted as a portal.
  EXPECT_TRUE(web_contents_impl->IsPortal());
  EXPECT_EQ(web_contents_impl->GetOuterWebContents(), portal_contents);
}

// Tests that the RenderFrameProxyHost is created and initialized when the
// portal is initialized.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, RenderFrameProxyHostCreated) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  Portal* portal = nullptr;
  PortalCreatedObserver portal_created_observer(main_frame);
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(ExecJs(main_frame,
                     JsReplace("var portal = document.createElement('portal');"
                               "portal.src = $1;"
                               "document.body.appendChild(portal);",
                               a_url)));
  portal = portal_created_observer.WaitUntilPortalCreated();
  WebContentsImpl* portal_contents = portal->GetPortalContents();
  RenderFrameProxyHost* proxy_host = portal_contents->GetFrameTree()
                                         ->root()
                                         ->render_manager()
                                         ->GetProxyToOuterDelegate();
  EXPECT_TRUE(proxy_host->is_render_frame_proxy_live());
}

// Tests that the portal's outer delegate frame tree node and any iframes
// inside the portal are deleted when the portal element is removed from the
// document.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, DetachPortal) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents->GetMainFrame();

  Portal* portal = nullptr;
  PortalCreatedObserver portal_created_observer(main_frame);
  GURL a_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  TestNavigationObserver navigation_observer(a_url);
  navigation_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(ExecJs(main_frame,
                     JsReplace("var portal = document.createElement('portal');"
                               "portal.src = $1;"
                               "document.body.appendChild(portal);",
                               a_url)));

  // Wait for portal to be created.
  portal = portal_created_observer.WaitUntilPortalCreated();
  WebContentsImpl* portal_contents = portal->GetPortalContents();
  FrameTreeNode* portal_main_frame_node =
      portal_contents->GetFrameTree()->root();
  // Wait for portal frame to navigate.
  navigation_observer.WaitForNavigationFinished();
  // Wait for inner iframe to navigate.
  TestNavigationObserver observer2(portal_contents);
  observer2.WaitForNavigationFinished();

  // Remove portal from document and wait for frames to be deleted.
  FrameDeletedObserver fdo1(portal_main_frame_node->render_manager()
                                ->GetOuterDelegateNode()
                                ->current_frame_host());
  FrameDeletedObserver fdo2(
      portal_main_frame_node->child_at(0)->current_frame_host());
  EXPECT_TRUE(ExecJs(main_frame, "document.body.removeChild(portal);"));
  fdo1.Wait();
  fdo2.Wait();
}

// This is for testing how portals interact with input hit testing. It is
// parameterized on the kind of viz hit testing used.
class PortalHitTestBrowserTest : public PortalBrowserTest,
                                 public ::testing::WithParamInterface<bool> {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PortalBrowserTest::SetUpCommandLine(command_line);
    IsolateAllSitesForTesting(command_line);
    const bool use_viz_hit_test_surface_layer = GetParam();
    if (use_viz_hit_test_surface_layer) {
      feature_list_.InitAndEnableFeature(
          features::kEnableVizHitTestSurfaceLayer);
    } else {
      feature_list_.InitAndDisableFeature(
          features::kEnableVizHitTestSurfaceLayer);
    }
  }

  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         PortalHitTestBrowserTest,
                         ::testing::Bool());

namespace {

// Fails the test if an input event is sent to the given RenderWidgetHost.
class FailOnInputEvent : public RenderWidgetHost::InputEventObserver {
 public:
  explicit FailOnInputEvent(RenderWidgetHostImpl* rwh)
      : rwh_(rwh->GetWeakPtr()) {
    rwh->AddInputEventObserver(this);
  }

  ~FailOnInputEvent() override {
    if (rwh_)
      rwh_->RemoveInputEventObserver(this);
  }

  void OnInputEvent(const blink::WebInputEvent& event) override {
    FAIL() << "Unexpected " << blink::WebInputEvent::GetName(event.GetType());
  }

 private:
  base::WeakPtr<RenderWidgetHostImpl> rwh_;
};

}  // namespace

// Tests that input events targeting the portal are only received by the parent
// renderer.
IN_PROC_BROWSER_TEST_P(PortalHitTestBrowserTest, DispatchInputEvent) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  // Create portal and wait for navigation.
  Portal* portal = nullptr;
  PortalCreatedObserver portal_created_observer(main_frame);
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  TestNavigationObserver navigation_observer(a_url);
  navigation_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(ExecJs(main_frame,
                     JsReplace("var portal = document.createElement('portal');"
                               "portal.src = $1;"
                               "document.body.appendChild(portal);",
                               a_url)));
  portal = portal_created_observer.WaitUntilPortalCreated();
  WebContentsImpl* portal_contents = portal->GetPortalContents();
  RenderFrameHostImpl* portal_frame = portal_contents->GetMainFrame();
  EXPECT_TRUE(static_cast<RenderWidgetHostViewBase*>(portal_frame->GetView())
                  ->IsRenderWidgetHostViewChildFrame());
  RenderWidgetHostViewChildFrame* portal_view =
      static_cast<RenderWidgetHostViewChildFrame*>(portal_frame->GetView());
  navigation_observer.Wait();
  WaitForHitTestData(portal_frame);

  FailOnInputEvent no_input_to_portal_frame(
      portal_frame->GetRenderWidgetHost());
  EXPECT_TRUE(ExecJs(main_frame,
                     "var clicked = false;"
                     "portal.onmousedown = _ => clicked = true;"));
  EXPECT_TRUE(ExecJs(portal_frame,
                     "var clicked = false;"
                     "document.body.onmousedown = _ => clicked = true;"));
  EXPECT_EQ(false, EvalJs(main_frame, "clicked"));
  EXPECT_EQ(false, EvalJs(portal_frame, "clicked"));

  // Route the mouse event.
  gfx::Point root_location =
      portal_view->TransformPointToRootCoordSpace(gfx::Point(5, 5));
  InputEventAckWaiter waiter(main_frame->GetRenderWidgetHost(),
                             blink::WebInputEvent::kMouseDown);
  SimulateRoutedMouseEvent(web_contents_impl, blink::WebInputEvent::kMouseDown,
                           blink::WebPointerProperties::Button::kLeft,
                           root_location);
  waiter.Wait();

  // Check that the click event was only received by the main frame.
  EXPECT_EQ(true, EvalJs(main_frame, "clicked"));
  EXPECT_EQ(false, EvalJs(portal_frame, "clicked"));
}

// Tests that input events performed over on OOPIF inside a portal are targeted
// to the portal's parent.
IN_PROC_BROWSER_TEST_P(PortalHitTestBrowserTest, NoInputToOOPIFInPortal) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  // Create portal and wait for navigation.
  // In the case of crbug.com/1002228 , this does not appear to reproduce if the
  // portal element is too small, so we give it an explicit size.
  Portal* portal = nullptr;
  PortalCreatedObserver portal_created_observer(main_frame);
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  TestNavigationObserver navigation_observer(a_url);
  navigation_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(ExecJs(main_frame,
                     JsReplace("var portal = document.createElement('portal');"
                               "portal.src = $1;"
                               "portal.style.width = '500px';"
                               "portal.style.height = '500px';"
                               "portal.style.border = 'solid';"
                               "document.body.appendChild(portal);",
                               a_url)));
  portal = portal_created_observer.WaitUntilPortalCreated();
  WebContentsImpl* portal_contents = portal->GetPortalContents();
  RenderFrameHostImpl* portal_frame = portal_contents->GetMainFrame();
  navigation_observer.Wait();
  WaitForHitTestData(portal_frame);

  // Add an out-of-process iframe to the portal.
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  TestNavigationObserver iframe_navigation_observer(portal_contents);
  EXPECT_TRUE(ExecJs(portal_frame,
                     JsReplace("var iframe = document.createElement('iframe');"
                               "iframe.src = $1;"
                               "document.body.appendChild(iframe);",
                               b_url)));
  iframe_navigation_observer.Wait();
  EXPECT_EQ(b_url, iframe_navigation_observer.last_navigation_url());
  RenderFrameHostImpl* portal_iframe =
      portal_frame->child_at(0)->current_frame_host();
  EXPECT_TRUE(static_cast<RenderWidgetHostViewBase*>(portal_iframe->GetView())
                  ->IsRenderWidgetHostViewChildFrame());
  RenderWidgetHostViewChildFrame* oopif_view =
      static_cast<RenderWidgetHostViewChildFrame*>(portal_iframe->GetView());
  EXPECT_NE(portal_frame->GetSiteInstance(), portal_iframe->GetSiteInstance());
  WaitForHitTestData(portal_iframe);

  FailOnInputEvent no_input_to_portal_frame(
      portal_frame->GetRenderWidgetHost());
  FailOnInputEvent no_input_to_oopif(portal_iframe->GetRenderWidgetHost());
  EXPECT_TRUE(ExecJs(main_frame,
                     "var clicked = false;"
                     "portal.onmousedown = _ => clicked = true;"));
  EXPECT_TRUE(ExecJs(portal_frame,
                     "var clicked = false;"
                     "document.body.onmousedown = _ => clicked = true;"));
  EXPECT_TRUE(ExecJs(portal_iframe,
                     "var clicked = false;"
                     "document.body.onmousedown = _ => clicked = true;"));

  // Route the mouse event.
  gfx::Point root_location =
      oopif_view->TransformPointToRootCoordSpace(gfx::Point(5, 5));
  InputEventAckWaiter waiter(main_frame->GetRenderWidgetHost(),
                             blink::WebInputEvent::kMouseDown);
  SimulateRoutedMouseEvent(web_contents_impl, blink::WebInputEvent::kMouseDown,
                           blink::WebPointerProperties::Button::kLeft,
                           root_location);
  waiter.Wait();

  // Check that the click event was only received by the main frame.
  EXPECT_EQ(true, EvalJs(main_frame, "clicked"));
  EXPECT_EQ(false, EvalJs(portal_frame, "clicked"));
  EXPECT_EQ(false, EvalJs(portal_iframe, "clicked"));
}

// Tests that async hit testing does not target portals.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, AsyncEventTargetingIgnoresPortals) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  // Create portal and wait for navigation.
  PortalCreatedObserver portal_created_observer(main_frame);
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  TestNavigationObserver navigation_observer(a_url);
  navigation_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(ExecJs(main_frame,
                     JsReplace("var portal = document.createElement('portal');"
                               "portal.src = $1;"
                               "document.body.appendChild(portal);",
                               a_url)));
  Portal* portal = portal_created_observer.WaitUntilPortalCreated();
  WebContentsImpl* portal_contents = portal->GetPortalContents();
  RenderFrameHostImpl* portal_frame = portal_contents->GetMainFrame();
  ASSERT_TRUE(static_cast<RenderWidgetHostViewBase*>(portal_frame->GetView())
                  ->IsRenderWidgetHostViewChildFrame());
  RenderWidgetHostViewChildFrame* portal_view =
      static_cast<RenderWidgetHostViewChildFrame*>(portal_frame->GetView());
  navigation_observer.Wait();
  WaitForHitTestData(portal_frame);

  viz::mojom::InputTargetClient* target_client =
      main_frame->GetRenderWidgetHost()->input_target_client();
  ASSERT_TRUE(target_client);

  gfx::PointF root_location =
      portal_view->TransformPointToRootCoordSpaceF(gfx::PointF(5, 5));

  // Query the renderer for the target widget. The root should claim the point
  // for itself, not the portal.
  base::RunLoop run_loop;
  base::OnceClosure quit_closure = run_loop.QuitClosure();
  viz::FrameSinkId received_frame_sink_id;
  target_client->FrameSinkIdAt(
      root_location, 0,
      base::BindLambdaForTesting(
          [&](const viz::FrameSinkId& id, const gfx::PointF& point) {
            received_frame_sink_id = id;
            std::move(quit_closure).Run();
          }));
  run_loop.Run();

  viz::FrameSinkId root_frame_sink_id =
      static_cast<RenderWidgetHostViewBase*>(main_frame->GetView())
          ->GetFrameSinkId();
  EXPECT_EQ(root_frame_sink_id, received_frame_sink_id)
      << "Note: The portal's FrameSinkId is " << portal_view->GetFrameSinkId();
}

// Tests that trying to navigate to a chrome:// URL kills the renderer.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, NavigateToChrome) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  // Create portal.
  PortalCreatedObserver portal_created_observer(main_frame);
  EXPECT_TRUE(ExecJs(main_frame,
                     "var portal = document.createElement('portal');"
                     "document.body.appendChild(portal);"));
  Portal* portal = portal_created_observer.WaitUntilPortalCreated();
  PortalInterceptorForTesting* portal_interceptor =
      PortalInterceptorForTesting::From(portal);

  // Try to navigate to chrome://settings and wait for the process to die.
  portal_interceptor->SetNavigateCallback(base::BindRepeating(
      [](Portal* portal, const GURL& url, blink::mojom::ReferrerPtr referrer,
         blink::mojom::Portal::NavigateCallback callback) {
        GURL chrome_url("chrome://settings");
        portal->Navigate(chrome_url, std::move(referrer), std::move(callback));
      },
      portal));
  RenderProcessHostKillWaiter kill_waiter(main_frame->GetProcess());
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  ignore_result(ExecJs(main_frame, JsReplace("portal.src = $1;", a_url)));

  EXPECT_EQ(bad_message::RPH_MOJO_PROCESS_ERROR, kill_waiter.Wait());
}

// Regression test for crbug.com/969714. Tests that receiving a touch ack
// from the predecessor after portal activation doesn't cause a crash.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, TouchAckAfterActivate) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  // Create portal and wait for navigation.
  Portal* portal = nullptr;
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  TestNavigationObserver navigation_observer(a_url);
  navigation_observer.StartWatchingNewWebContents();
  {
    PortalCreatedObserver portal_created_observer(main_frame);
    EXPECT_TRUE(ExecJs(
        main_frame, JsReplace("var portal = document.createElement('portal');"
                              "portal.src = $1;"
                              "document.body.appendChild(portal);"
                              "document.body.addEventListener('touchstart', "
                              "e => { portal.activate(); }, {passive: false});",
                              a_url)));
    portal = portal_created_observer.WaitUntilPortalCreated();
  }
  WebContentsImpl* portal_contents = portal->GetPortalContents();
  navigation_observer.Wait();

  PortalInterceptorForTesting* portal_interceptor =
      PortalInterceptorForTesting::From(portal);
  RenderWidgetHostImpl* render_widget_host = main_frame->GetRenderWidgetHost();
  RenderFrameHostImpl* portal_frame = portal_contents->GetMainFrame();
  RenderWidgetHostViewChildFrame* portal_view =
      static_cast<RenderWidgetHostViewChildFrame*>(portal_frame->GetView());
  InputEventAckWaiter input_event_ack_waiter(
      render_widget_host, blink::WebInputEvent::Type::kTouchStart);
  WaitForHitTestData(portal_frame);

  SyntheticTapGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.position =
      portal_view->TransformPointToRootCoordSpaceF(gfx::PointF(5, 5));

  std::unique_ptr<SyntheticTapGesture> gesture =
      std::make_unique<SyntheticTapGesture>(params);
  render_widget_host->QueueSyntheticGesture(
      std::move(gesture), base::Bind([](SyntheticGesture::Result) {}));
  portal_interceptor->WaitForActivate();
  EXPECT_EQ(portal_contents, shell()->web_contents());

  // Wait for a touch ack to be sent from the predecessor.
  input_event_ack_waiter.Wait();
}

// Regression test for crbug.com/973647. Tests that receiving a touch ack
// after activation and predecessor adoption doesn't cause a crash.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, TouchAckAfterActivateAndAdopt) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  // Create portal and wait for navigation.
  Portal* portal = nullptr;
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  TestNavigationObserver navigation_observer(a_url);
  navigation_observer.StartWatchingNewWebContents();
  {
    PortalCreatedObserver portal_created_observer(main_frame);
    const int TOUCH_ACK_DELAY_IN_MILLISECONDS = 500;
    EXPECT_TRUE(
        ExecJs(main_frame,
               JsReplace("var portal = document.createElement('portal');"
                         "portal.src = $1;"
                         "document.body.appendChild(portal);"
                         "document.body.addEventListener('touchstart', e => {"
                         "  portal.activate();"
                         "  var stop = performance.now() + $2;"
                         "  while (performance.now() < stop) {}"
                         "}, {passive: false});",
                         a_url, TOUCH_ACK_DELAY_IN_MILLISECONDS)));
    portal = portal_created_observer.WaitUntilPortalCreated();
  }
  WebContentsImpl* portal_contents = portal->GetPortalContents();
  navigation_observer.Wait();

  RenderFrameHostImpl* portal_frame = portal_contents->GetMainFrame();
  EXPECT_TRUE(ExecJs(portal_frame,
                     "window.addEventListener('portalactivate', e => {"
                     "  var portal = e.adoptPredecessor();"
                     "  document.body.appendChild(portal);"
                     "});"));
  WaitForHitTestData(portal_frame);

  PortalInterceptorForTesting* portal_interceptor =
      PortalInterceptorForTesting::From(portal);
  RenderWidgetHostImpl* render_widget_host = main_frame->GetRenderWidgetHost();
  InputEventAckWaiter input_event_ack_waiter(
      render_widget_host, blink::WebInputEvent::Type::kTouchStart);

  SyntheticTapGestureParams params;
  RenderWidgetHostViewChildFrame* portal_view =
      static_cast<RenderWidgetHostViewChildFrame*>(portal_frame->GetView());
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.position =
      portal_view->TransformPointToRootCoordSpaceF(gfx::PointF(5, 5));

  std::unique_ptr<SyntheticTapGesture> gesture =
      std::make_unique<SyntheticTapGesture>(params);
  {
    PortalCreatedObserver adoption_observer(portal_frame);
    render_widget_host->QueueSyntheticGesture(
        std::move(gesture), base::Bind([](SyntheticGesture::Result) {}));
    portal_interceptor->WaitForActivate();
    EXPECT_EQ(portal_contents, shell()->web_contents());
    // Wait for predecessor to be adopted.
    adoption_observer.WaitUntilPortalCreated();
  }

  // Wait for a touch ack to be sent from the predecessor.
  input_event_ack_waiter.Wait();
}

// Regression test for crbug.com/973647. Tests that receiving a touch ack
// after activation and reactivating a predecessor doesn't cause a crash.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, TouchAckAfterActivateAndReactivate) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  // Create portal and wait for navigation.
  Portal* portal = nullptr;
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  TestNavigationObserver navigation_observer(a_url);
  navigation_observer.StartWatchingNewWebContents();
  {
    PortalCreatedObserver portal_created_observer(main_frame);
    const int TOUCH_ACK_DELAY_IN_MILLISECONDS = 500;
    EXPECT_TRUE(
        ExecJs(main_frame,
               JsReplace("var portal = document.createElement('portal');"
                         "portal.src = $1;"
                         "document.body.appendChild(portal);"
                         "document.body.addEventListener('touchstart', e => {"
                         "  portal.activate();"
                         "  var stop = performance.now() + $2;"
                         "  while (performance.now() < stop) {}"
                         "}, {passive: false});"
                         "var stop = performance.now() + 500;"
                         "while (performance.now() < stop) {}",
                         a_url, TOUCH_ACK_DELAY_IN_MILLISECONDS)));
    portal = portal_created_observer.WaitUntilPortalCreated();
  }
  WebContentsImpl* portal_contents = portal->GetPortalContents();
  navigation_observer.Wait();

  RenderFrameHostImpl* portal_frame = portal_contents->GetMainFrame();
  EXPECT_TRUE(ExecJs(portal_frame,
                     "window.addEventListener('portalactivate', e => {"
                     "  var portal = e.adoptPredecessor();"
                     "  document.body.appendChild(portal);"
                     "  portal.activate();"
                     "});"));
  WaitForHitTestData(portal_frame);

  PortalInterceptorForTesting* portal_interceptor =
      PortalInterceptorForTesting::From(portal);
  RenderWidgetHostImpl* render_widget_host = main_frame->GetRenderWidgetHost();
  InputEventAckWaiter input_event_ack_waiter(
      render_widget_host, blink::WebInputEvent::Type::kTouchStart);

  SyntheticTapGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.position = gfx::PointF(20, 20);

  std::unique_ptr<SyntheticTapGesture> gesture =
      std::make_unique<SyntheticTapGesture>(params);

  Portal* predecessor_portal = nullptr;
  {
    PortalCreatedObserver adoption_observer(portal_frame);
    render_widget_host->QueueSyntheticGesture(
        std::move(gesture), base::Bind([](SyntheticGesture::Result) {}));
    portal_interceptor->WaitForActivate();
    EXPECT_EQ(portal_contents, shell()->web_contents());
    predecessor_portal = adoption_observer.WaitUntilPortalCreated();
  }

  portal_interceptor = PortalInterceptorForTesting::From(predecessor_portal);
  portal_interceptor->WaitForActivate();
  // Sanity check to see if the predecessor was reactivated.
  EXPECT_EQ(web_contents_impl, shell()->web_contents());

  // Wait for a touch ack to be sent from the predecessor.
  input_event_ack_waiter.Wait();
}

// TODO(crbug.com/985078): Fix on Mac.
#if !defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, TouchStateClearedBeforeActivation) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  // Create portal and wait for navigation.
  Portal* portal = nullptr;
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  TestNavigationObserver navigation_observer(a_url);
  navigation_observer.StartWatchingNewWebContents();
  {
    PortalCreatedObserver portal_created_observer(main_frame);
    EXPECT_TRUE(
        ExecJs(main_frame,
               JsReplace("var portal = document.createElement('portal');"
                         "portal.src = $1;"
                         "document.body.appendChild(portal);"
                         "document.body.addEventListener('touchstart', e => {"
                         "  portal.activate();"
                         "}, {passive: false});",
                         a_url)));
    portal = portal_created_observer.WaitUntilPortalCreated();
  }
  WebContentsImpl* portal_contents = portal->GetPortalContents();
  navigation_observer.Wait();

  RenderFrameHostImpl* portal_frame = portal_contents->GetMainFrame();
  EXPECT_TRUE(ExecJs(portal_frame,
                     "window.addEventListener('portalactivate', e => {"
                     "  var portal = e.adoptPredecessor();"
                     "  document.body.appendChild(portal);"
                     "  portal.activate();"
                     "});"));
  WaitForHitTestData(portal_frame);

  PortalInterceptorForTesting* portal_interceptor =
      PortalInterceptorForTesting::From(portal);
  RenderWidgetHostImpl* render_widget_host = main_frame->GetRenderWidgetHost();

  SyntheticTapGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.position = gfx::PointF(20, 20);

  // Activate the portal, and then wait for the predecessor to be reactivated.
  Portal* adopted_portal = nullptr;
  {
    PortalCreatedObserver adoption_observer(portal_frame);
    std::unique_ptr<SyntheticTapGesture> gesture =
        std::make_unique<SyntheticTapGesture>(params);
    InputEventAckWaiter input_event_ack_waiter(
        render_widget_host, blink::WebInputEvent::Type::kTouchCancel);
    render_widget_host->QueueSyntheticGestureCompleteImmediately(
        std::move(gesture));
    // Wait for synthetic cancel event to be sent.
    input_event_ack_waiter.Wait();
    portal_interceptor->WaitForActivate();
    EXPECT_EQ(portal_contents, shell()->web_contents());
    adopted_portal = adoption_observer.WaitUntilPortalCreated();
  }
  PortalInterceptorForTesting* adopted_portal_interceptor =
      PortalInterceptorForTesting::From(adopted_portal);
  adopted_portal_interceptor->WaitForActivate();
  // Sanity check to see if the predecessor was reactivated.
  EXPECT_EQ(web_contents_impl, shell()->web_contents());

  InputEventAckWaiter input_event_ack_waiter(
      render_widget_host, blink::WebInputEvent::Type::kTouchStart);
  std::unique_ptr<SyntheticTapGesture> gesture =
      std::make_unique<SyntheticTapGesture>(params);
  render_widget_host->QueueSyntheticGesture(
      std::move(gesture), base::Bind([](SyntheticGesture::Result) {}));
  // Waits for touch to be acked. If touch state wasn't cleared before initial
  // activation, a DCHECK will be hit before the ack is sent.
  input_event_ack_waiter.Wait();
}
#endif

// TODO(crbug.com/985078): Fix on Mac.
#if !defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, GestureCleanedUpBeforeActivation) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  // Create portal and wait for navigation.
  Portal* portal = nullptr;
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  TestNavigationObserver navigation_observer(a_url);
  navigation_observer.StartWatchingNewWebContents();
  {
    PortalCreatedObserver portal_created_observer(main_frame);
    EXPECT_TRUE(
        ExecJs(main_frame,
               JsReplace("var portal = document.createElement('portal');"
                         "portal.src = $1;"
                         "document.body.appendChild(portal);"
                         "document.body.addEventListener('touchstart', e => {"
                         "  portal.activate();"
                         "}, {once: true});",
                         a_url)));
    portal = portal_created_observer.WaitUntilPortalCreated();
  }
  WebContentsImpl* portal_contents = portal->GetPortalContents();
  navigation_observer.Wait();

  RenderFrameHostImpl* portal_frame = portal_contents->GetMainFrame();
  EXPECT_TRUE(ExecJs(portal_frame,
                     "window.addEventListener('portalactivate', e => {"
                     "  var portal = e.adoptPredecessor();"
                     "  document.body.appendChild(portal);"
                     "  portal.activate(); "
                     "});"));
  WaitForHitTestData(portal_frame);

  PortalInterceptorForTesting* portal_interceptor =
      PortalInterceptorForTesting::From(portal);
  RenderWidgetHostImpl* render_widget_host = main_frame->GetRenderWidgetHost();

  SyntheticTapGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.position = gfx::PointF(20, 20);
  params.duration_ms = 1;

  // Simulate a tap and activate the portal.
  Portal* adopted_portal = nullptr;
  {
    PortalCreatedObserver adoption_observer(portal_frame);
    std::unique_ptr<SyntheticTapGesture> gesture =
        std::make_unique<SyntheticTapGesture>(params);
    render_widget_host->QueueSyntheticGestureCompleteImmediately(
        std::move(gesture));
    portal_interceptor->WaitForActivate();
    EXPECT_EQ(portal_contents, shell()->web_contents());
    adopted_portal = adoption_observer.WaitUntilPortalCreated();
  }

  // Wait for predecessor to be reactivated.
  PortalInterceptorForTesting* adopted_portal_interceptor =
      PortalInterceptorForTesting::From(adopted_portal);
  adopted_portal_interceptor->WaitForActivate();
  EXPECT_EQ(web_contents_impl, shell()->web_contents());

  // Simulate another tap.
  InputEventAckWaiter input_event_ack_waiter(
      render_widget_host, blink::WebInputEvent::Type::kGestureTap);
  auto gesture = std::make_unique<SyntheticTapGesture>(params);
  render_widget_host->QueueSyntheticGesture(
      std::move(gesture), base::Bind([](SyntheticGesture::Result) {}));
  // Wait for the tap gesture ack. If the initial gesture wasn't cleaned up, the
  // new gesture created will cause an error in the gesture validator.
  input_event_ack_waiter.Wait();
}
#endif

// Touch input transfer is only implemented in the content layer for Aura.
#if defined(USE_AURA)
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, TouchInputTransferAcrossActivation) {
  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("portal.test", "/portals/scroll.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();
  RenderWidgetHostImpl* render_widget_host = main_frame->GetRenderWidgetHost();

  // Create portal and wait for navigation.
  Portal* portal = nullptr;
  GURL portal_url(embedded_test_server()->GetURL(
      "portal.test", "/portals/scroll-portal.html"));
  TestNavigationObserver navigation_observer(portal_url);
  navigation_observer.StartWatchingNewWebContents();
  {
    PortalCreatedObserver portal_created_observer(main_frame);

    EXPECT_TRUE(ExecJs(
        main_frame, JsReplace("var portal = document.createElement('portal');"
                              "portal.src = $1;"
                              "document.body.appendChild(portal);",
                              portal_url)));
    portal = portal_created_observer.WaitUntilPortalCreated();
  }
  WebContentsImpl* portal_contents = portal->GetPortalContents();
  RenderFrameHostImpl* portal_frame = portal_contents->GetMainFrame();
  navigation_observer.Wait();
  WaitForHitTestData(portal_frame);

  // Create and dispatch a synthetic scroll to trigger activation.
  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.anchor =
      gfx::PointF(50, main_frame->GetView()->GetViewBounds().height() - 100);
  params.distances.push_back(-gfx::Vector2d(0, 100));

  std::unique_ptr<SyntheticSmoothScrollGesture> gesture =
      std::make_unique<SyntheticSmoothScrollGesture>(params);
  base::RunLoop run_loop;
  render_widget_host->QueueSyntheticGesture(
      std::move(gesture),
      base::BindLambdaForTesting([&](SyntheticGesture::Result result) {
        EXPECT_EQ(SyntheticGesture::Result::GESTURE_FINISHED, result);
        run_loop.Quit();
      }));
  run_loop.Run();

  // Check if the activated page scrolled.
  EXPECT_NE(0, EvalJs(portal_frame, "window.scrollY"));
}
#endif

// TODO(crbug.com/1010675): Test fails flakily.
// Touch input transfer is only implemented in the content layer for Aura.
#if defined(USE_AURA)
IN_PROC_BROWSER_TEST_F(PortalBrowserTest,
                       TouchInputTransferAcrossReactivation) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "portal.test",
                   "/portals/touch-input-transfer-across-reactivation.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();
  RenderWidgetHostImpl* render_widget_host = main_frame->GetRenderWidgetHost();

  // Create portal and wait for navigation.
  Portal* portal = nullptr;
  GURL portal_url(embedded_test_server()->GetURL(
      "portal.test", "/portals/reactivate-predecessor.html"));
  TestNavigationObserver navigation_observer(portal_url);
  navigation_observer.StartWatchingNewWebContents();
  {
    PortalCreatedObserver portal_created_observer(main_frame);

    EXPECT_TRUE(ExecJs(
        main_frame, JsReplace("var portal = document.createElement('portal');"
                              "portal.src = $1;"
                              "document.body.appendChild(portal);",
                              portal_url)));
    portal = portal_created_observer.WaitUntilPortalCreated();
  }
  navigation_observer.Wait();
  WaitForHitTestData(main_frame);

  PortalInterceptorForTesting* portal_interceptor =
      PortalInterceptorForTesting::From(portal);

  // Create and dispatch a synthetic scroll to trigger activation.
  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.anchor =
      gfx::PointF(50, main_frame->GetView()->GetViewBounds().height() - 100);
  params.distances.push_back(-gfx::Vector2d(0, 250));
  params.speed_in_pixels_s = 200;

  std::unique_ptr<SyntheticSmoothScrollGesture> gesture =
      std::make_unique<SyntheticSmoothScrollGesture>(params);
  base::RunLoop run_loop;
  render_widget_host->QueueSyntheticGesture(
      std::move(gesture),
      base::BindLambdaForTesting([&](SyntheticGesture::Result result) {
        EXPECT_EQ(SyntheticGesture::Result::GESTURE_FINISHED, result);
        run_loop.Quit();
      }));
  // Portal should activate when the gesture begins.
  portal_interceptor->WaitForActivate();
  // Wait till the scroll gesture finishes.
  run_loop.Run();
  // The predecessor should have been reactivated (we should be back to the
  // starting page).
  EXPECT_EQ(web_contents_impl, shell()->web_contents());
  // The starting page should have scrolled.
  // NOTE: This assumes that the scroll gesture is long enough that touch events
  // are still sent after the predecessor is reactivated.
  int scroll_y_after_portal_activate =
      EvalJs(main_frame, "scrollYAfterPortalActivate").ExtractInt();
  EXPECT_LT(scroll_y_after_portal_activate,
            EvalJs(main_frame, "window.scrollY"));
}
#endif

// Tests that the outer FrameTreeNode is deleted after activation.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, FrameDeletedAfterActivation) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  Portal* portal = nullptr;
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  TestNavigationObserver navigation_observer(a_url);
  navigation_observer.StartWatchingNewWebContents();
  {
    PortalCreatedObserver portal_created_observer(main_frame);
    EXPECT_TRUE(ExecJs(
        main_frame, JsReplace("var portal = document.createElement('portal');"
                              "portal.src = $1;"
                              "document.body.appendChild(portal);",
                              a_url)));
    portal = portal_created_observer.WaitUntilPortalCreated();
  }
  WebContentsImpl* portal_contents = portal->GetPortalContents();
  navigation_observer.Wait();

  FrameTreeNode* outer_frame_tree_node = FrameTreeNode::GloballyFindByID(
      portal_contents->GetOuterDelegateFrameTreeNodeId());
  EXPECT_TRUE(outer_frame_tree_node);

  EXPECT_TRUE(ExecJs(portal_contents->GetMainFrame(),
                     "window.onportalactivate = e => "
                     "document.body.appendChild(e.adoptPredecessor());"));

  {
    FrameDeletedObserver observer(outer_frame_tree_node->current_frame_host());
    PortalCreatedObserver portal_created_observer(
        portal_contents->GetMainFrame());
    ExecuteScriptAsync(main_frame,
                       "document.querySelector('portal').activate();");
    observer.Wait();

    // Observes the creation of a new portal due to the adoption of the
    // predecessor during the activate event.
    // TODO(lfg): We only wait for the adoption callback to avoid a race
    // receiving a sync IPC in a nested message loop while the browser is
    // sending out another sync IPC to the GPU process.
    // https://crbug.com/976367.
    portal_created_observer.WaitUntilPortalCreated();
  }
}

// Tests that activating a portal at the same time as it is being removed
// doesn't crash the browser.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, RemovePortalWhenUnloading) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  // Create a container for the portal.
  EXPECT_TRUE(ExecJs(main_frame,
                     "var div = document.createElement('div');"
                     "document.body.appendChild(div);"));

  // Create portal.
  PortalCreatedObserver portal_created_observer(main_frame);
  EXPECT_TRUE(ExecJs(main_frame,
                     "var portal = document.createElement('portal');"
                     "div.appendChild(portal);"));

  // Add a same-origin iframe in the same div as the portal that activates the
  // portal on its unload handler.
  EXPECT_TRUE(
      ExecJs(main_frame,
             "var iframe = document.createElement('iframe');"
             "iframe.src = 'about:blank';"
             "div.appendChild(iframe);"
             "iframe.contentWindow.onunload = () => portal.activate();"));

  // Remove the div from the document. This destroys the portal's WebContents
  // and should destroy the Portal object as well, so that the activate message
  // is not processed.
  EXPECT_TRUE(ExecJs(main_frame, "div.remove();"));
}

// Tests that a portal can navigate while orphaned.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, OrphanedNavigation) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  Portal* portal = nullptr;
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  TestNavigationObserver navigation_observer(a_url);
  navigation_observer.StartWatchingNewWebContents();
  {
    PortalCreatedObserver portal_created_observer(main_frame);
    EXPECT_TRUE(ExecJs(
        main_frame, JsReplace("var portal = document.createElement('portal');"
                              "portal.src = $1;"
                              "document.body.appendChild(portal);",
                              a_url)));
    portal = portal_created_observer.WaitUntilPortalCreated();
  }
  PortalInterceptorForTesting* portal_interceptor =
      PortalInterceptorForTesting::From(portal);
  WebContentsImpl* portal_contents = portal->GetPortalContents();
  navigation_observer.Wait();

  // Block the activate callback so that the predecessor portal stays orphaned.
  EXPECT_TRUE(ExecJs(portal_contents->GetMainFrame(),
                     "window.onportalactivate = e => { while(true) {} };"));

  // Activate the portal and navigate the predecessor.
  TestNavigationObserver main_frame_navigation_observer(web_contents_impl);
  ExecuteScriptAsync(main_frame,
                     "document.querySelector('portal').activate();"
                     "window.location.reload()");
  portal_interceptor->WaitForActivate();
  main_frame_navigation_observer.Wait();
}

// Tests that the browser doesn't crash if the renderer tries to create the
// PortalHost after the parent renderer dropped the portal.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest,
                       AccessPortalHostAfterPortalDestruction) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  Portal* portal = nullptr;
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  TestNavigationObserver navigation_observer(a_url);
  navigation_observer.StartWatchingNewWebContents();
  {
    PortalCreatedObserver portal_created_observer(main_frame);
    EXPECT_TRUE(ExecJs(
        main_frame, JsReplace("var portal = document.createElement('portal');"
                              "portal.src = $1;"
                              "document.body.appendChild(portal);",
                              a_url)));
    portal = portal_created_observer.WaitUntilPortalCreated();
  }
  WebContentsImpl* portal_contents = portal->GetPortalContents();
  RenderFrameHostImpl* portal_frame = portal_contents->GetMainFrame();
  navigation_observer.Wait();

  // Simulate the portal being dropped, but not the destruction of the
  // WebContents.
  portal->GetBindingForTesting()->SwapImplForTesting(nullptr);

  // Get the portal renderer to access the WebContents.
  RenderProcessHostKillWaiter kill_waiter(portal_frame->GetProcess());
  ExecuteScriptAsync(portal_frame,
                     "window.portalHost.postMessage('message', '*');");
  EXPECT_EQ(bad_message::RPH_MOJO_PROCESS_ERROR, kill_waiter.Wait());
}

class PortalOOPIFBrowserTest : public PortalBrowserTest {
 protected:
  PortalOOPIFBrowserTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolateAllSitesForTesting(command_line);
  }
};

// Tests that creating and destroying OOPIFs inside the portal works as
// intended.
IN_PROC_BROWSER_TEST_F(PortalOOPIFBrowserTest, OOPIFInsidePortal) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  // Create portal and wait for navigation.
  PortalCreatedObserver portal_created_observer(main_frame);
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  TestNavigationObserver navigation_observer(a_url);
  navigation_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(ExecJs(main_frame,
                     JsReplace("var portal = document.createElement('portal');"
                               "portal.src = $1;"
                               "document.body.appendChild(portal);",
                               a_url)));
  Portal* portal = portal_created_observer.WaitUntilPortalCreated();
  WebContentsImpl* portal_contents = portal->GetPortalContents();
  RenderFrameHostImpl* portal_main_frame = portal_contents->GetMainFrame();
  navigation_observer.Wait();

  // Add an out-of-process iframe to the portal.
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  TestNavigationObserver iframe_navigation_observer(portal_contents);
  EXPECT_TRUE(ExecJs(portal_main_frame,
                     JsReplace("var iframe = document.createElement('iframe');"
                               "iframe.src = $1;"
                               "document.body.appendChild(iframe);",
                               b_url)));
  iframe_navigation_observer.Wait();
  EXPECT_EQ(b_url, iframe_navigation_observer.last_navigation_url());
  RenderFrameHostImpl* portal_iframe =
      portal_main_frame->child_at(0)->current_frame_host();
  EXPECT_NE(portal_main_frame->GetSiteInstance(),
            portal_iframe->GetSiteInstance());

  // Remove the OOPIF from the portal.
  RenderFrameDeletedObserver deleted_observer(portal_iframe);
  EXPECT_TRUE(
      ExecJs(portal_main_frame, "document.querySelector('iframe').remove();"));
  deleted_observer.WaitUntilDeleted();
}

}  // namespace content
