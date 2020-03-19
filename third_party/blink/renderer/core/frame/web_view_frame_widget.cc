// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "third_party/blink/renderer/core/frame/web_view_frame_widget.h"

#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/widget/widget_base.h"

namespace blink {

WebViewFrameWidget::WebViewFrameWidget(
    util::PassKey<WebFrameWidget>,
    WebWidgetClient& client,
    WebViewImpl& web_view,
    CrossVariantMojoAssociatedRemote<mojom::blink::FrameWidgetHostInterfaceBase>
        frame_widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::FrameWidgetInterfaceBase>
        frame_widget,
    CrossVariantMojoAssociatedRemote<mojom::blink::WidgetHostInterfaceBase>
        widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::WidgetInterfaceBase>
        widget)
    : WebFrameWidgetBase(client,
                         std::move(frame_widget_host),
                         std::move(frame_widget),
                         std::move(widget_host),
                         std::move(widget)),
      web_view_(&web_view),
      self_keep_alive_(PERSISTENT_FROM_HERE, this) {
  web_view_->SetMainFrameWidgetBase(this);
}

WebViewFrameWidget::~WebViewFrameWidget() = default;

void WebViewFrameWidget::Close() {
  GetPage()->WillCloseAnimationHost(nullptr);
  // Closing the WebViewFrameWidget happens in response to the local main frame
  // being detached from the Page/WebViewImpl.
  web_view_->SetMainFrameWidgetBase(nullptr);
  web_view_ = nullptr;
  WebFrameWidgetBase::Close();
  self_keep_alive_.Clear();
}

WebSize WebViewFrameWidget::Size() {
  return web_view_->Size();
}

void WebViewFrameWidget::Resize(const WebSize& size) {
  web_view_->Resize(size);
}

void WebViewFrameWidget::DidEnterFullscreen() {
  web_view_->DidEnterFullscreen();
}

void WebViewFrameWidget::DidExitFullscreen() {
  web_view_->DidExitFullscreen();
}

void WebViewFrameWidget::SetSuppressFrameRequestsWorkaroundFor704763Only(
    bool suppress_frame_requests) {
  web_view_->SetSuppressFrameRequestsWorkaroundFor704763Only(
      suppress_frame_requests);
}

void WebViewFrameWidget::DidBeginFrame() {
  web_view_->DidBeginFrame();
}

void WebViewFrameWidget::BeginMainFrame(base::TimeTicks last_frame_time) {
  web_view_->BeginFrame(last_frame_time);
}

void WebViewFrameWidget::BeginUpdateLayers() {
  web_view_->BeginUpdateLayers();
}

void WebViewFrameWidget::EndUpdateLayers() {
  web_view_->EndUpdateLayers();
}

void WebViewFrameWidget::BeginCommitCompositorFrame() {
  web_view_->BeginCommitCompositorFrame();
}

void WebViewFrameWidget::EndCommitCompositorFrame(
    base::TimeTicks commit_start_time) {
  web_view_->EndCommitCompositorFrame(commit_start_time);
}

void WebViewFrameWidget::RecordStartOfFrameMetrics() {
  web_view_->RecordStartOfFrameMetrics();
}

void WebViewFrameWidget::RecordEndOfFrameMetrics(
    base::TimeTicks frame_begin_time,
    cc::ActiveFrameSequenceTrackers trackers) {
  web_view_->RecordEndOfFrameMetrics(frame_begin_time, trackers);
}

std::unique_ptr<cc::BeginMainFrameMetrics>
WebViewFrameWidget::GetBeginMainFrameMetrics() {
  return web_view_->GetBeginMainFrameMetrics();
}

void WebViewFrameWidget::UpdateLifecycle(WebLifecycleUpdate requested_update,
                                         DocumentUpdateReason reason) {
  web_view_->UpdateLifecycle(requested_update, reason);
}

void WebViewFrameWidget::ThemeChanged() {
  web_view_->ThemeChanged();
}

WebInputEventResult WebViewFrameWidget::HandleInputEvent(
    const WebCoalescedInputEvent& event) {
  return web_view_->HandleInputEvent(event);
}

WebInputEventResult WebViewFrameWidget::DispatchBufferedTouchEvents() {
  return web_view_->DispatchBufferedTouchEvents();
}

void WebViewFrameWidget::SetCursorVisibilityState(bool is_visible) {
  web_view_->SetCursorVisibilityState(is_visible);
}

void WebViewFrameWidget::OnFallbackCursorModeToggled(bool is_on) {
  web_view_->OnFallbackCursorModeToggled(is_on);
}

void WebViewFrameWidget::ApplyViewportChanges(
    const ApplyViewportChangesArgs& args) {
  web_view_->ApplyViewportChanges(args);
}

void WebViewFrameWidget::RecordManipulationTypeCounts(
    cc::ManipulationInfo info) {
  web_view_->RecordManipulationTypeCounts(info);
}
void WebViewFrameWidget::SendOverscrollEventFromImplSide(
    const gfx::Vector2dF& overscroll_delta,
    cc::ElementId scroll_latched_element_id) {
  web_view_->SendOverscrollEventFromImplSide(overscroll_delta,
                                             scroll_latched_element_id);
}
void WebViewFrameWidget::SendScrollEndEventFromImplSide(
    cc::ElementId scroll_latched_element_id) {
  web_view_->SendScrollEndEventFromImplSide(scroll_latched_element_id);
}

void WebViewFrameWidget::MouseCaptureLost() {
  web_view_->MouseCaptureLost();
}

void WebViewFrameWidget::SetFocus(bool enable) {
  web_view_->SetFocus(enable);
}

bool WebViewFrameWidget::SelectionBounds(WebRect& anchor,
                                         WebRect& focus) const {
  return web_view_->SelectionBounds(anchor, focus);
}

WebURL WebViewFrameWidget::GetURLForDebugTrace() {
  return web_view_->GetURLForDebugTrace();
}

void WebViewFrameWidget::DidDetachLocalFrameTree() {
  web_view_->DidDetachLocalMainFrame();
}

WebInputMethodController*
WebViewFrameWidget::GetActiveWebInputMethodController() const {
  return web_view_->GetActiveWebInputMethodController();
}

bool WebViewFrameWidget::ScrollFocusedEditableElementIntoView() {
  return web_view_->ScrollFocusedEditableElementIntoView();
}

void WebViewFrameWidget::SetRootLayer(scoped_refptr<cc::Layer> root_layer) {
  if (!web_view_->does_composite()) {
    DCHECK(!root_layer);
    return;
  }
  cc::LayerTreeHost* layer_tree_host = widget_base_->LayerTreeHost();
  layer_tree_host->SetRootLayer(root_layer);
  web_view_->DidChangeRootLayer(!!root_layer);
}

WebHitTestResult WebViewFrameWidget::HitTestResultAt(const gfx::Point& point) {
  return web_view_->HitTestResultAt(point);
}

HitTestResult WebViewFrameWidget::CoreHitTestResultAt(const gfx::Point& point) {
  return web_view_->CoreHitTestResultAt(point);
}

void WebViewFrameWidget::ZoomToFindInPageRect(
    const WebRect& rect_in_root_frame) {
  web_view_->ZoomToFindInPageRect(rect_in_root_frame);
}

void WebViewFrameWidget::Trace(Visitor* visitor) {
  WebFrameWidgetBase::Trace(visitor);
}

PageWidgetEventHandler* WebViewFrameWidget::GetPageWidgetEventHandler() {
  return web_view_.get();
}

LocalFrameView* WebViewFrameWidget::GetLocalFrameViewForAnimationScrolling() {
  // Scrolling for the root frame is special we need to pass null indicating
  // we are at the top of the tree when setting up the Animation. Which will
  // cause ownership of the timeline and animation host.
  // See ScrollingCoordinator::AnimationHostInitialized.
  return nullptr;
}

}  // namespace blink
