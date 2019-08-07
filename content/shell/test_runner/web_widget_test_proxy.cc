// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/web_widget_test_proxy.h"

#include "content/renderer/compositor/compositor_dependencies.h"
#include "content/renderer/compositor/layer_tree_view.h"
#include "content/renderer/input/widget_input_handler_manager.h"
#include "content/shell/test_runner/test_interfaces.h"
#include "content/shell/test_runner/test_runner.h"
#include "content/shell/test_runner/test_runner_for_specific_view.h"
#include "content/shell/test_runner/web_test_delegate.h"
#include "content/shell/test_runner/web_test_interfaces.h"
#include "content/shell/test_runner/web_view_test_proxy.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_page_popup.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_widget.h"

namespace test_runner {

WebWidgetTestProxy::~WebWidgetTestProxy() = default;

void WebWidgetTestProxy::BeginMainFrame(base::TimeTicks frame_time) {
  // This must happen before we run BeginMainFrame() in the base class, which
  // will change states. TestFinished() wants to grab the current state.
  GetTestRunner()->FinishTestIfReady();

  RenderWidget::BeginMainFrame(frame_time);
}

void WebWidgetTestProxy::RequestDecode(
    const cc::PaintImage& image,
    base::OnceCallback<void(bool)> callback) {
  RenderWidget::RequestDecode(image, std::move(callback));

  // In web tests the request does not actually cause a commit, because the
  // compositor is scheduled by the test runner to avoid flakiness. So for this
  // case we must request a main frame the way blink would.
  //
  // Pass true for |do_raster| to ensure the compositor is actually run, rather
  // than just doing the main frame animate step.
  if (GetTestRunner()->TestIsRunning())
    ScheduleAnimationInternal(/*do_raster=*/true);
}

void WebWidgetTestProxy::RequestPresentation(
    PresentationTimeCallback callback) {
  RenderWidget::RequestPresentation(std::move(callback));

  // Single threaded web tests must explicitly schedule commits.
  //
  // Pass true for |do_raster| to ensure the compositor is actually run, rather
  // than just doing the main frame animate step. That way we know it will
  // submit a frame and later trigger the presentation callback in order to make
  // progress in the test.
  if (GetTestRunner()->TestIsRunning())
    ScheduleAnimationInternal(/*do_raster=*/true);
}

void WebWidgetTestProxy::ScheduleAnimation() {
  if (GetTestRunner()->TestIsRunning())
    ScheduleAnimationInternal(GetTestRunner()->animation_requires_raster());
}

void WebWidgetTestProxy::ScheduleAnimationInternal(bool do_raster) {
  // When using threaded compositing, have the RenderWidget schedule a request
  // for a frame, as we use the compositor's scheduler. Otherwise the testing
  // WebWidgetClient schedules it.
  // Note that for WebWidgetTestProxy the RenderWidget is subclassed to override
  // the WebWidgetClient, so we must call up to the base class RenderWidget
  // explicitly here to jump out of the test harness as intended.
  if (RenderWidget::compositor_deps()->GetCompositorImplThreadTaskRunner()) {
    RenderWidget::ScheduleAnimation();
    return;
  }

  // If an animation already scheduled we'll make it composite, otherwise we'll
  // schedule another animation step with composite now.
  composite_requested_ |= do_raster;

  if (!animation_scheduled_) {
    animation_scheduled_ = true;
    GetWebViewTestProxy()->delegate()->PostDelayedTask(
        base::BindOnce(&WebWidgetTestProxy::AnimateNow,
                       weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(1));
  }
}

bool WebWidgetTestProxy::RequestPointerLock(blink::WebLocalFrame*) {
  return GetViewTestRunner()->RequestPointerLock();
}

void WebWidgetTestProxy::RequestPointerUnlock() {
  return GetViewTestRunner()->RequestPointerUnlock();
}

bool WebWidgetTestProxy::IsPointerLocked() {
  return GetViewTestRunner()->isPointerLocked();
}

void WebWidgetTestProxy::SetToolTipText(const blink::WebString& text,
                                        blink::WebTextDirection hint) {
  RenderWidget::SetToolTipText(text, hint);
  GetTestRunner()->setToolTipText(text);
}

void WebWidgetTestProxy::StartDragging(network::mojom::ReferrerPolicy policy,
                                       const blink::WebDragData& data,
                                       blink::WebDragOperationsMask mask,
                                       const SkBitmap& drag_image,
                                       const gfx::Point& image_offset) {
  GetTestRunner()->setDragImage(drag_image);

  // When running a test, we need to fake a drag drop operation otherwise
  // Windows waits for real mouse events to know when the drag is over.
  event_sender()->DoDragDrop(data, mask);
}

WebViewTestProxy* WebWidgetTestProxy::GetWebViewTestProxy() {
  if (delegate()) {
    // TODO(https://crbug.com/545684): Because WebViewImpl still inherits
    // from WebWidget and infact, before a frame is attached, IS actually
    // the WebWidget used in blink, it is not possible to walk the object
    // relations in the blink side to find the associated RenderViewImpl
    // consistently. Thus, here, just directly cast the delegate(). Since
    // all creations of RenderViewImpl in the test_runner layer haved been
    // shimmed to return a WebViewTestProxy, it is safe to downcast here.
    return static_cast<WebViewTestProxy*>(delegate());
  } else {
    blink::WebWidget* web_widget = GetWebWidget();
    CHECK(web_widget->IsWebFrameWidget());
    blink::WebView* web_view =
        static_cast<blink::WebFrameWidget*>(web_widget)->LocalRoot()->View();

    content::RenderView* render_view =
        content::RenderView::FromWebView(web_view);
    // RenderViews are always WebViewTestProxy within the test_runner namespace.
    return static_cast<WebViewTestProxy*>(render_view);
  }
}

void WebWidgetTestProxy::Reset() {
  event_sender_.Reset();
}

void WebWidgetTestProxy::BindTo(blink::WebLocalFrame* frame) {
  event_sender_.Install(frame);
}

void WebWidgetTestProxy::EndSyntheticGestures() {
  widget_input_handler_manager()->InvokeInputProcessedCallback();
}

TestRunnerForSpecificView* WebWidgetTestProxy::GetViewTestRunner() {
  return GetWebViewTestProxy()->view_test_runner();
}

TestRunner* WebWidgetTestProxy::GetTestRunner() {
  return GetWebViewTestProxy()->test_interfaces()->GetTestRunner();
}

static void DoComposite(content::RenderWidget* widget, bool do_raster) {
  if (!widget->layer_tree_view()->layer_tree_host()->IsVisible())
    return;

  if (widget->in_synchronous_composite_for_testing()) {
    // Web tests can use a nested message loop to pump frames while inside a
    // frame, but the compositor does not support this. In this case, we only
    // run blink's lifecycle updates.
    widget->BeginMainFrame(base::TimeTicks::Now());
    widget->UpdateVisualState();
    return;
  }

  // Ensure that there is damage so that the compositor submits, and the display
  // compositor draws this frame.
  if (do_raster) {
    content::LayerTreeView* layer_tree_view = widget->layer_tree_view();
    layer_tree_view->layer_tree_host()->SetNeedsCommitWithForcedRedraw();
  }

  widget->set_in_synchronous_composite_for_testing(true);
  widget->layer_tree_view()->layer_tree_host()->Composite(
      base::TimeTicks::Now(), do_raster);
  widget->set_in_synchronous_composite_for_testing(false);
}

void WebWidgetTestProxy::SynchronouslyComposite(bool do_raster) {
  DCHECK(!compositor_deps()->GetCompositorImplThreadTaskRunner());
  DCHECK(!layer_tree_view()
              ->layer_tree_host()
              ->GetSettings()
              .single_thread_proxy_scheduler);

  DoComposite(this, do_raster);

  // If the RenderWidget is for the main frame, we also composite the current
  // PagePopup afterward.
  //
  // TODO(danakj): This means that an OOPIF's popup, which is attached to a
  // WebView without a main frame, would have no opportunity to execute this
  // method call.
  if (delegate()) {
    blink::WebView* view = GetWebViewTestProxy()->webview();
    if (blink::WebPagePopup* popup = view->GetPagePopup()) {
      auto* popup_render_widget =
          static_cast<RenderWidget*>(popup->GetClientForTesting());
      DoComposite(popup_render_widget, do_raster);
    }
  }
}

void WebWidgetTestProxy::AnimateNow() {
  // For child local roots, it's possible that the backing WebWidget gets
  // closed between the ScheduleAnimation() call and this execution
  // leading to a nullptr.  This happens because child local roots are
  // owned by RenderFrames which drops the WebWidget before executing the
  // Close() call that would invalidate the |weak_factory_| canceling the
  // scheduled calls to AnimateNow(). In main frames, the WebWidget is
  // dropped synchronously by Close() avoiding the problem.
  //
  // Ideally there would not be this divergence between frame types, and/or
  // there would be a hook for signaling a stop to the animation. As this is
  // not an ideal work, returning early when GetWebWidget() returns nullptr
  // is good enough for a fake impl. Also, unicorns are mythical. Sorry.
  if (!GetWebWidget())
    return;

  bool do_raster = composite_requested_;
  animation_scheduled_ = false;
  composite_requested_ = false;
  SynchronouslyComposite(do_raster);
}

}  // namespace test_runner
