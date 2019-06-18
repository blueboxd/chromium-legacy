// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/frame_host/render_frame_host_manager.h"
#include "content/browser/frame_host/render_frame_proxy_host.h"
#include "content/browser/portal/portal.h"
#include "content/browser/renderer_host/input/synthetic_tap_gesture.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame.mojom-test-utils.h"
#include "content/public/browser/web_contents_delegate.h"
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
#include "third_party/blink/public/mojom/portal/portal.mojom-test-utils.h"
#include "third_party/blink/public/mojom/portal/portal.mojom.h"
#include "url/url_constants.h"

using testing::_;

namespace content {

// The PortalInterceptorForTesting can be used in tests to inspect Portal IPCs.
class PortalInterceptorForTesting final
    : public blink::mojom::PortalInterceptorForTesting {
 public:
  static PortalInterceptorForTesting* Create(
      RenderFrameHostImpl* render_frame_host_impl,
      mojo::PendingAssociatedReceiver<blink::mojom::Portal> receiver,
      mojo::AssociatedRemote<blink::mojom::PortalClient> client);
  static PortalInterceptorForTesting* From(content::Portal* portal);

  void Activate(blink::TransferableMessage data,
                ActivateCallback callback) override {
    portal_activated_ = true;

    if (run_loop_) {
      run_loop_->Quit();
      run_loop_ = nullptr;
    }

    // |this| can be destroyed after Activate() is called.
    portal_->Activate(std::move(data), std::move(callback));
  }

  void Navigate(const GURL& url, blink::mojom::ReferrerPtr referrer) override {
    if (navigate_callback_) {
      navigate_callback_.Run(url, std::move(referrer));
      return;
    }

    portal_->Navigate(url, std::move(referrer));
  }

  void WaitForActivate() {
    if (portal_activated_)
      return;

    base::RunLoop run_loop;
    run_loop_ = &run_loop;
    run_loop.Run();
  }

  // Test getters.
  content::Portal* GetPortal() { return portal_.get(); }
  WebContents* GetPortalContents() { return portal_->GetPortalContents(); }

  // IPC callbacks
  base::RepeatingCallback<void(const GURL&, blink::mojom::ReferrerPtr)>
      navigate_callback_;

 private:
  PortalInterceptorForTesting(RenderFrameHostImpl* render_frame_host_impl)
      : portal_(content::Portal::CreateForTesting(render_frame_host_impl)) {}

  blink::mojom::Portal* GetForwardingInterface() override {
    return portal_.get();
  }

  std::unique_ptr<content::Portal> portal_;
  bool portal_activated_ = false;
  base::RunLoop* run_loop_ = nullptr;
};

// static
PortalInterceptorForTesting* PortalInterceptorForTesting::Create(
    RenderFrameHostImpl* render_frame_host_impl,
    mojo::PendingAssociatedReceiver<blink::mojom::Portal> receiver,
    mojo::AssociatedRemote<blink::mojom::PortalClient> client) {
  auto test_portal_ptr =
      base::WrapUnique(new PortalInterceptorForTesting(render_frame_host_impl));
  PortalInterceptorForTesting* test_portal = test_portal_ptr.get();
  test_portal->GetPortal()->SetBindingForTesting(
      mojo::MakeStrongAssociatedBinding<blink::mojom::Portal>(
          std::move(test_portal_ptr), std::move(receiver)));
  test_portal->GetPortal()->SetClientForTesting(std::move(client));
  return test_portal;
}

// static
PortalInterceptorForTesting* PortalInterceptorForTesting::From(
    content::Portal* portal) {
  blink::mojom::Portal* impl = portal->GetBindingForTesting()->impl();
  auto* interceptor = static_cast<PortalInterceptorForTesting*>(impl);
  CHECK_NE(static_cast<blink::mojom::Portal*>(portal), impl);
  CHECK_EQ(interceptor->GetPortal(), portal);
  return interceptor;
}

// The PortalCreatedObserver observes portal creations on
// |render_frame_host_impl|. This observer can be used to monitor for multiple
// Portal creations on the same RenderFrameHost, by repeatedly calling
// WaitUntilPortalCreated().
class PortalCreatedObserver : public mojom::FrameHostInterceptorForTesting {
 public:
  explicit PortalCreatedObserver(RenderFrameHostImpl* render_frame_host_impl)
      : render_frame_host_impl_(render_frame_host_impl) {
    render_frame_host_impl_->frame_host_binding_for_testing()
        .SwapImplForTesting(this);
  }

  ~PortalCreatedObserver() override {}

  FrameHost* GetForwardingInterface() override {
    return render_frame_host_impl_;
  }

  void CreatePortal(
      mojo::PendingAssociatedReceiver<blink::mojom::Portal> portal,
      mojo::PendingAssociatedRemote<blink::mojom::PortalClient> client,
      CreatePortalCallback callback) override {
    PortalInterceptorForTesting* portal_interceptor =
        PortalInterceptorForTesting::Create(
            render_frame_host_impl_, std::move(portal),
            mojo::AssociatedRemote<blink::mojom::PortalClient>(
                std::move(client)));
    portal_ = portal_interceptor->GetPortal();
    RenderFrameProxyHost* proxy_host = portal_->CreateProxyAndAttachPortal();
    std::move(callback).Run(proxy_host->GetRoutingID(), portal_->portal_token(),
                            portal_->GetDevToolsFrameToken());

    if (run_loop_)
      run_loop_->Quit();
  }

  Portal* WaitUntilPortalCreated() {
    Portal* portal = portal_;
    if (portal) {
      portal_ = nullptr;
      return portal;
    }

    base::RunLoop run_loop;
    run_loop_ = &run_loop;
    run_loop.Run();
    run_loop_ = nullptr;

    portal = portal_;
    portal_ = nullptr;
    return portal;
  }

 private:
  RenderFrameHostImpl* render_frame_host_impl_;
  base::RunLoop* run_loop_ = nullptr;
  Portal* portal_ = nullptr;
};

// The PortalAdoptionObserver observes calls to AdoptPortal on
// RenderFrameHostImpl. This observer should be used to wait for a single
// call to AdoptPortal.
class PortalAdoptionObserver : public mojom::FrameHostInterceptorForTesting {
 public:
  explicit PortalAdoptionObserver(RenderFrameHostImpl* render_frame_host_impl)
      : render_frame_host_impl_(render_frame_host_impl) {
    render_frame_host_impl_->frame_host_binding_for_testing()
        .SwapImplForTesting(this);
  }

  ~PortalAdoptionObserver() override {}

  FrameHost* GetForwardingInterface() override {
    return render_frame_host_impl_;
  }

  void AdoptPortal(const base::UnguessableToken& portal_token,
                   AdoptPortalCallback callback) override {
    Portal* portal = Portal::FromToken(portal_token);
    DCHECK(portal);
    RenderFrameProxyHost* proxy_host = portal->CreateProxyAndAttachPortal();
    std::move(callback).Run(proxy_host->GetRoutingID(),
                            portal->GetDevToolsFrameToken());
    portal_adopted_ = true;
    if (run_loop_)
      run_loop_->Quit();
  }

  void WaitUntilPortalAdopted() {
    if (portal_adopted_)
      return;
    base::RunLoop run_loop;
    run_loop_ = &run_loop;
    run_loop.Run();
    run_loop_ = nullptr;
  }

 private:
  RenderFrameHostImpl* render_frame_host_impl_;
  base::RunLoop* run_loop_ = nullptr;
  bool portal_adopted_ = false;
};

class PortalBrowserTest : public ContentBrowserTest {
 protected:
  PortalBrowserTest() {}

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(blink::features::kPortals);
    ContentBrowserTest::SetUp();
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
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(
      ExecJs(main_frame,
             base::StringPrintf("var portal = document.createElement('portal');"
                                "portal.src = '%s';"
                                "document.body.appendChild(portal);",
                                a_url.spec().c_str())));

  PortalInterceptorForTesting* portal_interceptor =
      PortalInterceptorForTesting::From(
          portal_created_observer.WaitUntilPortalCreated());
  WebContents* portal_contents = portal_interceptor->GetPortalContents();
  EXPECT_NE(nullptr, portal_contents);
  EXPECT_NE(portal_contents->GetLastCommittedURL(), a_url);

  // The portal should not have navigated yet, so we can observe the Portal's
  // first navigation.
  TestNavigationObserver navigation_observer(portal_contents);
  navigation_observer.Wait();
  EXPECT_EQ(navigation_observer.last_navigation_url(), a_url);
  EXPECT_EQ(portal_contents->GetLastCommittedURL(), a_url);

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
  WebContentsImpl* portal_contents = portal->GetPortalContents();
  EXPECT_NE(nullptr, portal_contents);
  EXPECT_NE(portal_contents, shell()->web_contents());

  // The portal should not have navigated yet, so we can observe the Portal's
  // first navigation.
  TestNavigationObserver navigation_observer(portal_contents);
  navigation_observer.Wait();

  RenderFrameHostImpl* portal_frame = portal_contents->GetMainFrame();
  EXPECT_TRUE(ExecJs(portal_frame,
                     "window.addEventListener('portalactivate', e => { "
                     "  var portal = e.adoptPredecessor(); "
                     "  document.body.appendChild(portal); "
                     "});"));

  {
    PortalAdoptionObserver adoption_observer(portal_frame);
    EXPECT_TRUE(ExecJs(main_frame,
                       "portal.activate().then(() => { "
                       "  document.body.removeChild(portal); "
                       "});"));
    portal_interceptor->WaitForActivate();
    adoption_observer.WaitUntilPortalAdopted();
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

  // The portal should not have navigated yet, wait for the first navigation.
  TestNavigationObserver navigation_observer(portal_contents);
  navigation_observer.Wait();

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

// Tests that input events targeting the portal are only received by the parent
// renderer.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, DispatchInputEvent) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  // Create portal and wait for navigation.
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
  RenderFrameHostImpl* portal_frame = portal_contents->GetMainFrame();
  EXPECT_TRUE(static_cast<RenderWidgetHostViewBase*>(portal_frame->GetView())
                  ->IsRenderWidgetHostViewChildFrame());
  RenderWidgetHostViewChildFrame* portal_view =
      static_cast<RenderWidgetHostViewChildFrame*>(portal_frame->GetView());
  TestNavigationObserver navigation_observer(portal_contents);
  navigation_observer.Wait();
  WaitForHitTestDataOrChildSurfaceReady(portal_frame);

  // Create listeners for both widgets.
  RenderWidgetHostMouseEventMonitor main_frame_monitor(
      main_frame->GetRenderWidgetHost());
  RenderWidgetHostMouseEventMonitor portal_frame_monitor(
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
  main_frame_monitor.ResetEventReceived();
  portal_frame_monitor.ResetEventReceived();
  InputEventAckWaiter waiter(main_frame->GetRenderWidgetHost(),
                             blink::WebInputEvent::kMouseDown);
  SimulateRoutedMouseEvent(web_contents_impl, blink::WebInputEvent::kMouseDown,
                           blink::WebPointerProperties::Button::kLeft,
                           root_location);
  waiter.Wait();

  // Check that the click event was only received by the main frame.
  EXPECT_TRUE(main_frame_monitor.EventWasReceived());
  EXPECT_FALSE(portal_frame_monitor.EventWasReceived());
  EXPECT_EQ(true, EvalJs(main_frame, "clicked"));
  EXPECT_EQ(false, EvalJs(portal_frame, "clicked"));
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
  TestNavigationObserver navigation_observer(portal_contents);
  navigation_observer.Wait();
  WaitForHitTestDataOrChildSurfaceReady(portal_frame);

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
  WebContentsImpl* portal_contents = portal->GetPortalContents();

  // Try to navigate to chrome://settings and wait for the process to die.
  portal_interceptor->navigate_callback_ = base::BindRepeating(
      [](Portal* portal, const GURL& url, blink::mojom::ReferrerPtr referrer) {
        GURL chrome_url("chrome://settings");
        portal->Navigate(chrome_url, std::move(referrer));
      },
      portal);
  RenderProcessHostKillWaiter kill_waiter(
      portal_contents->GetMainFrame()->GetProcess());
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  ignore_result(ExecJs(main_frame, JsReplace("portal.src = $1;", a_url)));

  EXPECT_EQ(base::nullopt, kill_waiter.Wait());
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
  PortalCreatedObserver portal_created_observer(main_frame);
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(ExecJs(
      main_frame, JsReplace("var portal = document.createElement('portal');"
                            "portal.src = $1;"
                            "document.body.appendChild(portal);"
                            "document.body.addEventListener('touchstart', "
                            "e => { portal.activate(); }, {passive: false});",
                            a_url)));
  Portal* portal = portal_created_observer.WaitUntilPortalCreated();
  WebContentsImpl* portal_contents = portal->GetPortalContents();

  // The portal should not have navigated yet, wait for the first navigation.
  TestNavigationObserver navigation_observer(portal_contents);
  navigation_observer.Wait();

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
  render_widget_host->QueueSyntheticGesture(
      std::move(gesture), base::Bind([](SyntheticGesture::Result) {}));
  portal_interceptor->WaitForActivate();
  EXPECT_EQ(portal_contents, shell()->web_contents());

  // Wait for a touch ack to be sent from the predecessor.
  input_event_ack_waiter.Wait();
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
  EXPECT_TRUE(ExecJs(main_frame,
                     JsReplace("var portal = document.createElement('portal');"
                               "portal.src = $1;"
                               "document.body.appendChild(portal);",
                               a_url)));
  Portal* portal = portal_created_observer.WaitUntilPortalCreated();
  WebContentsImpl* portal_contents = portal->GetPortalContents();
  RenderFrameHostImpl* portal_main_frame = portal_contents->GetMainFrame();
  TestNavigationObserver portal_navigation_observer(portal_contents);
  portal_navigation_observer.Wait();

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
