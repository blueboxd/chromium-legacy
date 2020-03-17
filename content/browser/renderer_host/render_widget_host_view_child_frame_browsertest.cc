// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/portal/portal.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/view_messages.h"
#include "content/common/widget_messages.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/portal/portal_created_observer.h"
#include "content/test/test_content_browser_client.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"
#include "ui/gfx/geometry/size.h"

namespace content {

class RenderWidgetHostViewChildFrameBrowserTest : public ContentBrowserTest {
 public:
  RenderWidgetHostViewChildFrameBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolateAllSitesForTesting(command_line);

    scoped_feature_list_.InitAndEnableFeature(blink::features::kPortals);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  // Tests that the FrameSinkId of each child frame has been updated by the
  // RenderFrameProxy.
  void CheckFrameSinkId(RenderFrameHost* render_frame_host) {
    RenderWidgetHostViewBase* child_view =
        static_cast<RenderFrameHostImpl*>(render_frame_host)
            ->GetRenderWidgetHost()
            ->GetView();
    // Only interested in updated FrameSinkIds on child frames.
    if (!child_view || !child_view->IsRenderWidgetHostViewChildFrame())
      return;

    // Ensure that the received viz::FrameSinkId was correctly set on the child
    // frame.
    viz::FrameSinkId actual_frame_sink_id_ = child_view->GetFrameSinkId();
    EXPECT_EQ(expected_frame_sink_id_, actual_frame_sink_id_);

    // The viz::FrameSinkID will be replaced while the test blocks for
    // navigation. It should differ from the information stored in the child's
    // RenderWidgetHost.
    EXPECT_NE(base::checked_cast<uint32_t>(
                  child_view->GetRenderWidgetHost()->GetProcess()->GetID()),
              actual_frame_sink_id_.client_id());
    EXPECT_NE(base::checked_cast<uint32_t>(
                  child_view->GetRenderWidgetHost()->GetRoutingID()),
              actual_frame_sink_id_.sink_id());
  }

  Portal* CreatePortalToUrl(WebContentsImpl* host_contents,
                            GURL portal_url,
                            int number_of_navigations = 1) {
    EXPECT_GE(number_of_navigations, 1);
    RenderFrameHostImpl* main_frame = host_contents->GetMainFrame();

    // Create portal and wait for navigation.
    PortalCreatedObserver portal_created_observer(main_frame);
    TestNavigationObserver navigation_observer(nullptr, number_of_navigations);
    navigation_observer.set_wait_event(
        TestNavigationObserver::WaitEvent::kNavigationFinished);
    navigation_observer.StartWatchingNewWebContents();
    EXPECT_TRUE(ExecJs(
        main_frame, JsReplace("{"
                              "  let portal = document.createElement('portal');"
                              "  portal.src = $1;"
                              "  portal.setAttribute('id', 'portal');"
                              "  document.body.appendChild(portal);"
                              "}",
                              portal_url)));
    Portal* portal = portal_created_observer.WaitUntilPortalCreated();
    navigation_observer.StopWatchingNewWebContents();

    WebContentsImpl* portal_contents = portal->GetPortalContents();
    EXPECT_TRUE(portal_contents);

    navigation_observer.WaitForNavigationFinished();
    EXPECT_TRUE(WaitForLoadStop(portal_contents));

    return portal;
  }

  void GiveItSomeTime() {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }

  void set_expected_frame_sink_id(viz::FrameSinkId frame_sink_id) {
    expected_frame_sink_id_ = frame_sink_id;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  viz::FrameSinkId expected_frame_sink_id_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewChildFrameBrowserTest);
};

// Tests that the screen is properly reflected for RWHVChildFrame.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewChildFrameBrowserTest, Screen) {
  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  // Load cross-site page into iframe.
  GURL cross_site_url(
      embedded_test_server()->GetURL("foo.com", "/title2.html"));
  NavigateFrameToURL(root->child_at(0), cross_site_url);

  int main_frame_screen_width =
      ExecuteScriptAndGetValue(shell()->web_contents()->GetMainFrame(),
                               "window.screen.width")
          .GetInt();
  EXPECT_NE(main_frame_screen_width, 0);

  auto check_screen_width = [&](RenderFrameHost* frame_host) {
    int width =
        ExecuteScriptAndGetValue(frame_host, "window.screen.width").GetInt();
    EXPECT_EQ(width, main_frame_screen_width);
  };
  shell()->web_contents()->ForEachFrame(
      base::BindLambdaForTesting(check_screen_width));
}

class OutgoingVisualPropertiesIPCWatcher {
 public:
  OutgoingVisualPropertiesIPCWatcher(
      RenderProcessHostImpl* rph,
      FrameTreeNode* root,
      base::RepeatingCallback<void(const VisualProperties&)> callback)
      : rph_(rph), root_(root), callback_(std::move(callback)) {
    rph_->SetIpcSendWatcherForTesting(
        base::BindRepeating(&OutgoingVisualPropertiesIPCWatcher::OnMessage,
                            base::Unretained(this)));
  }
  ~OutgoingVisualPropertiesIPCWatcher() {
    rph_->SetIpcSendWatcherForTesting(base::NullCallback());
  }

 private:
  bool IsMessageForFrameTreeWidget(int routing_id, FrameTreeNode* node) {
    auto* render_widget_host =
        node->current_frame_host()->GetRenderWidgetHost();
    if (routing_id == render_widget_host->GetRoutingID())
      return true;
    for (size_t i = 0; i < node->child_count(); ++i) {
      if (IsMessageForFrameTreeWidget(routing_id, node->child_at(i)))
        return true;
    }
    return false;
  }
  void OnMessage(const IPC::Message& message) {
    if (!IsMessageForFrameTreeWidget(message.routing_id(), root_))
      return;
    IPC_BEGIN_MESSAGE_MAP(OutgoingVisualPropertiesIPCWatcher, message)
      IPC_MESSAGE_HANDLER(WidgetMsg_UpdateVisualProperties, ProcessMessage)
    IPC_END_MESSAGE_MAP()
  }

  void ProcessMessage(const VisualProperties& props) { callback_.Run(props); }

  RenderProcessHostImpl* const rph_;
  FrameTreeNode* const root_;
  base::RepeatingCallback<void(const VisualProperties&)> callback_;
};

// Auto-resize is only implemented for Ash and GuestViews. So we need to inject
// an implementation that actually resizes the top level widget.
class AutoResizeWebContentsDelegate : public WebContentsDelegate {
  void ResizeDueToAutoResize(WebContents* web_contents,
                             const gfx::Size& new_size) override {
    RenderWidgetHostView* view =
        web_contents->GetTopLevelRenderWidgetHostView();
    view->SetSize(new_size);
  }
};

// Test that the |visible_viewport_size| from the top frame is propagated to all
// local roots (aka RenderWidgets):
// a) Initially upon adding the child frame (we create this scenario by
// navigating a child on b.com to c.com, where we already have a RenderProcess
// active for c.com).
// b) When resizing the top level widget.
// c) When auto-resize is enabled for the top level main frame and the renderer
// resizes the top level widget.
// d) When auto-resize is enabled for the nested main frame and the renderer
// resizes the nested widget.
// See https://crbug.com/726743 and https://crbug.com/1050635.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewChildFrameBrowserTest,
                       VisualPropertiesPropagation_VisibleViewportSize) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,c)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  auto* web_contents = static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = web_contents->GetFrameTree()->root();
  RenderWidgetHostView* root_view =
      root->current_frame_host()->GetRenderWidgetHost()->GetView();

  Portal* portal = CreatePortalToUrl(web_contents, main_url);
  WebContentsImpl* nested_contents = portal->GetPortalContents();
  FrameTreeNode* nested_root = nested_contents->GetFrameTree()->root();
  RenderWidgetHostView* nested_root_view =
      nested_root->current_frame_host()->GetRenderWidgetHost()->GetView();

  // Watch processes for a.com and c.com, as we will throw away b.com when we
  // navigate it below.
  auto* root_rph = static_cast<RenderProcessHostImpl*>(
      root->current_frame_host()->GetProcess());
  auto* child_rph = static_cast<RenderProcessHostImpl*>(
      root->child_at(1)->current_frame_host()->GetProcess());
  ASSERT_NE(root_rph, child_rph);

  auto* nested_root_rph = static_cast<RenderProcessHostImpl*>(
      nested_root->current_frame_host()->GetProcess());
  auto* nested_child_rph = static_cast<RenderProcessHostImpl*>(
      nested_root->child_at(1)->current_frame_host()->GetProcess());
  ASSERT_NE(nested_root_rph, nested_child_rph);

  const gfx::Size initial_size = root_view->GetVisibleViewportSize();
  const gfx::Size nested_initial_size =
      nested_root_view->GetVisibleViewportSize();
  ASSERT_NE(initial_size, nested_initial_size);

  // We should see the top level widget's size in the visible_viewport_size
  // in both local roots. When a child local root is added in the parent
  // renderer, the value is propagated down through the browser to the child
  // renderer's RenderWidget.
  //
  // This property is not directly visible in the renderer, so we can only
  // check that the value is sent to the appropriate RenderWidget.
  {
    base::RunLoop loop;

    gfx::Size child_visible_viewport_size;
    OutgoingVisualPropertiesIPCWatcher child_watcher(
        child_rph, root,
        base::BindLambdaForTesting([&](const VisualProperties& props) {
          child_visible_viewport_size = props.visible_viewport_size;

          if (child_visible_viewport_size == initial_size)
            loop.Quit();
        }));

    GURL cross_site_url(
        embedded_test_server()->GetURL("c.com", "/title2.html"));
    NavigateFrameToURL(root->child_at(0), cross_site_url);

    // Wait to see the size sent to the child RenderWidget.
    loop.Run();

    // The child widget was also informed of the same size.
    EXPECT_EQ(initial_size, child_visible_viewport_size);
  }

  // Same check as abvoe but for a nested WebContents.
  {
    base::RunLoop loop;

    gfx::Size child_visible_viewport_size;
    OutgoingVisualPropertiesIPCWatcher child_watcher(
        nested_child_rph, nested_root,
        base::BindLambdaForTesting([&](const VisualProperties& props) {
          child_visible_viewport_size = props.visible_viewport_size;

          if (child_visible_viewport_size == nested_initial_size)
            loop.Quit();
        }));

    GURL cross_site_url(
        embedded_test_server()->GetURL("c.com", "/title2.html"));
    NavigateFrameToURL(nested_root->child_at(0), cross_site_url);

    // Wait to see the size sent to the child RenderWidget.
    loop.Run();

    // The child widget was also informed of the same size.
    EXPECT_EQ(nested_initial_size, child_visible_viewport_size);
  }

// This part of the test does not work well on Android, for a few reasons:
// 1. RenderWidgetHostViewAndroid can not be resized, the Java objects need to
// be resized somehow through ui::ViewAndroid.
// 2. AutoResize on Android does not size to the min/max bounds specified, it
// ends up ignoring them and sizing to the screen (I think).
// Luckily this test is verifying interactions and behaviour of
// RenderWidgetHostImpl - RenderWidget - RenderFrameProxy -
// CrossProcessFrameConnector, and this isn't Android-specific code.
#if !defined(OS_ANDROID)

  // Resize the top level widget to cause its |visible_viewport_size| to be
  // changed. The change should propagate down to the child RenderWidget.
  {
    base::RunLoop loop;

    const gfx::Size resize_to(initial_size.width() - 10,
                              initial_size.height() - 10);

    gfx::Size root_visible_viewport_size;
    gfx::Size child_visible_viewport_size;
    OutgoingVisualPropertiesIPCWatcher root_watcher(
        root_rph, root,
        base::BindLambdaForTesting([&](const VisualProperties& props) {
          root_visible_viewport_size = props.visible_viewport_size;
        }));
    OutgoingVisualPropertiesIPCWatcher child_watcher(
        child_rph, root,
        base::BindLambdaForTesting([&](const VisualProperties& props) {
          child_visible_viewport_size = props.visible_viewport_size;

          if (child_visible_viewport_size == resize_to)
            loop.Quit();
        }));

    root_view->SetSize(resize_to);

    // Wait to see both RenderWidgets receive the message.
    loop.Run();

    // The top level widget was resized.
    EXPECT_EQ(resize_to, root_visible_viewport_size);
    // The child widget was also informed of the same size.
    EXPECT_EQ(resize_to, child_visible_viewport_size);
  }

  // Same check as above but resizing the nested WebContents' main frame
  // instead.
  // Resize the top level widget to cause its |visible_viewport_size| to be
  // changed. The change should propagate down to the child RenderWidget.
  {
    base::RunLoop loop;

    const gfx::Size resize_to(nested_initial_size.width() - 10,
                              nested_initial_size.height() - 10);

    gfx::Size root_visible_viewport_size;
    gfx::Size child_visible_viewport_size;
    OutgoingVisualPropertiesIPCWatcher root_watcher(
        nested_root_rph, nested_root,
        base::BindLambdaForTesting([&](const VisualProperties& props) {
          root_visible_viewport_size = props.visible_viewport_size;
        }));
    OutgoingVisualPropertiesIPCWatcher child_watcher(
        nested_child_rph, nested_root,
        base::BindLambdaForTesting([&](const VisualProperties& props) {
          child_visible_viewport_size = props.visible_viewport_size;

          if (child_visible_viewport_size == resize_to)
            loop.Quit();
        }));

    EXPECT_TRUE(ExecJs(
        root->current_frame_host(),
        JsReplace("document.getElementById('portal').style.width = '$1px';"
                  "document.getElementById('portal').style.height = '$2px';",
                  resize_to.width(), resize_to.height())));

    // Wait to see both RenderWidgets receive the message.
    loop.Run();

    // The top level widget was resized.
    EXPECT_EQ(resize_to, root_visible_viewport_size);
    // The child widget was also informed of the same size.
    EXPECT_EQ(resize_to, child_visible_viewport_size);
  }

  // Informs the top-level frame it can auto-resize. It will shrink down to its
  // minimum window size based on the empty content we've loaded, which is
  // 100x100.
  //
  // When the renderer resizes, thanks to our AutoResizeWebContentsDelegate
  // the top level widget will resize. That size will be reflected in the
  // visible_viewport_size which is sent back through the top level RenderWidget
  // to propagte down to child local roots.
  //
  // This property is not directly visible in the renderer, so we can only
  // check that the value is sent to both RenderWidgets.
  {
    base::RunLoop loop;

    const gfx::Size auto_resize_to(105, 100);

    gfx::Size root_visible_viewport_size;
    gfx::Size child_visible_viewport_size;
    OutgoingVisualPropertiesIPCWatcher root_watcher(
        root_rph, root,
        base::BindLambdaForTesting([&](const VisualProperties& props) {
          root_visible_viewport_size = props.visible_viewport_size;
        }));
    OutgoingVisualPropertiesIPCWatcher child_watcher(
        child_rph, root,
        base::BindLambdaForTesting([&](const VisualProperties& props) {
          child_visible_viewport_size = props.visible_viewport_size;

          if (child_visible_viewport_size == auto_resize_to)
            loop.Quit();
        }));

    // Replace the WebContentsDelegate so that we can use the auto-resize
    // changes to adjust the size of the top widget.
    WebContentsDelegate* old_delegate = shell()->web_contents()->GetDelegate();
    AutoResizeWebContentsDelegate resize_delegate;
    shell()->web_contents()->SetDelegate(&resize_delegate);

    root_view->EnableAutoResize(auto_resize_to, auto_resize_to);

    // Wait for the renderer side to resize itself and the RenderWidget
    // waterfall to pass the new |visible_viewport_size| down.
    loop.Run();

    // The top level widget was resized to match the auto-resized renderer.
    EXPECT_EQ(auto_resize_to, root_visible_viewport_size);
    // The child widget was also informed of the same size.
    EXPECT_EQ(auto_resize_to, child_visible_viewport_size);

    shell()->web_contents()->SetDelegate(old_delegate);
  }

  // TODO(danakj): We'd like to run the same check as above but tell the main
  // frame of the nested WebContents that it can auto-resize. However this seems
  // to get through to the main frame's RenderWidget and propagate correctly but
  // no size change occurs in the renderer.
#endif
}

// Validate that OOPIFs receive presentation feedbacks.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewChildFrameBrowserTest,
                       PresentationFeedback) {
  base::HistogramTester histogram_tester;
  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  // Load cross-site page into iframe.
  GURL cross_site_url(
      embedded_test_server()->GetURL("foo.com", "/title2.html"));
  NavigateFrameToURL(root->child_at(0), cross_site_url);

  auto* child_rwh_impl =
      root->child_at(0)->current_frame_host()->GetRenderWidgetHost();
  // Hide the frame and make it visible again, to force it to record the
  // tab-switch time, which is generated from presentation-feedback.
  child_rwh_impl->WasHidden();
  child_rwh_impl->WasShown(RecordContentToVisibleTimeRequest{
      base::TimeTicks::Now(), /* destination_is_loaded */ true,
      /* destination_is_frozen */ false, /* show_reason_tab_switching */ true,
      /* show_reason_unoccluded */ false,
      /* show_reason_bfcache_restore */ false});
  // Force the child to submit a new frame.
  ASSERT_TRUE(ExecuteScript(root->child_at(0)->current_frame_host(),
                            "document.write('Force a new frame.');"));
  do {
    FetchHistogramsFromChildProcesses();
    GiveItSomeTime();
  } while (histogram_tester.GetAllSamples("MPArch.RWH_TabSwitchPaintDuration")
               .size() != 1);
}

}  // namespace content
