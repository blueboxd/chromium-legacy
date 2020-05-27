// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/exported/web_external_widget_impl.h"

#include "cc/trees/layer_tree_host.h"
#include "cc/trees/ukm_manager.h"
#include "third_party/blink/public/platform/scheduler/web_render_widget_scheduling_state.h"
#include "third_party/blink/renderer/platform/widget/widget_base.h"

namespace blink {

std::unique_ptr<WebExternalWidget> WebExternalWidget::Create(
    WebExternalWidgetClient* client,
    const blink::WebURL& debug_url,
    CrossVariantMojoAssociatedRemote<mojom::blink::WidgetHostInterfaceBase>
        widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::WidgetInterfaceBase>
        widget) {
  return std::make_unique<WebExternalWidgetImpl>(
      client, debug_url, std::move(widget_host), std::move(widget));
}

WebExternalWidgetImpl::WebExternalWidgetImpl(
    WebExternalWidgetClient* client,
    const WebURL& debug_url,
    CrossVariantMojoAssociatedRemote<mojom::blink::WidgetHostInterfaceBase>
        widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::WidgetInterfaceBase>
        widget)
    : client_(client),
      debug_url_(debug_url),
      widget_base_(std::make_unique<WidgetBase>(this,
                                                std::move(widget_host),
                                                std::move(widget))) {
  DCHECK(client_);
}

WebExternalWidgetImpl::~WebExternalWidgetImpl() = default;

cc::LayerTreeHost* WebExternalWidgetImpl::InitializeCompositing(
    cc::TaskGraphRunner* task_graph_runner,
    const cc::LayerTreeSettings& settings,
    std::unique_ptr<cc::UkmRecorderFactory> ukm_recorder_factory) {
  widget_base_->InitializeCompositing(task_graph_runner, settings,
                                      std::move(ukm_recorder_factory));
  return widget_base_->LayerTreeHost();
}

void WebExternalWidgetImpl::Close(
    scoped_refptr<base::SingleThreadTaskRunner> cleanup_runner,
    base::OnceCallback<void()> cleanup_task) {
  widget_base_->Shutdown(std::move(cleanup_runner), std::move(cleanup_task));
  widget_base_.reset();
}

void WebExternalWidgetImpl::SetCompositorVisible(bool visible) {
  widget_base_->SetCompositorVisible(visible);
}

WebHitTestResult WebExternalWidgetImpl::HitTestResultAt(const gfx::PointF&) {
  NOTIMPLEMENTED();
  return {};
}

WebURL WebExternalWidgetImpl::GetURLForDebugTrace() {
  return debug_url_;
}

WebSize WebExternalWidgetImpl::Size() {
  return size_;
}

void WebExternalWidgetImpl::Resize(const WebSize& size) {
  if (size_ == size)
    return;
  size_ = size;
  client_->DidResize(gfx::Size(size));
}

WebInputEventResult WebExternalWidgetImpl::HandleInputEvent(
    const WebCoalescedInputEvent& coalesced_event) {
  return client_->HandleInputEvent(coalesced_event);
}

WebInputEventResult WebExternalWidgetImpl::DispatchBufferedTouchEvents() {
  return client_->DispatchBufferedTouchEvents();
}

scheduler::WebRenderWidgetSchedulingState*
WebExternalWidgetImpl::RendererWidgetSchedulingState() {
  return widget_base_->RendererWidgetSchedulingState();
}

void WebExternalWidgetImpl::SetCursor(const ui::Cursor& cursor) {
  widget_base_->SetCursor(cursor);
}

bool WebExternalWidgetImpl::HandlingInputEvent() {
  return widget_base_->input_handler().handling_input_event();
}

void WebExternalWidgetImpl::SetHandlingInputEvent(bool handling) {
  widget_base_->input_handler().set_handling_input_event(handling);
}

void WebExternalWidgetImpl::ProcessInputEventSynchronously(
    const WebCoalescedInputEvent& event,
    HandledEventCallback callback) {
  widget_base_->input_handler().HandleInputEvent(event, std::move(callback));
}

void WebExternalWidgetImpl::DidOverscrollForTesting(
    const gfx::Vector2dF& overscroll_delta,
    const gfx::Vector2dF& accumulated_overscroll,
    const gfx::PointF& position,
    const gfx::Vector2dF& velocity) {
  cc::OverscrollBehavior overscroll_behavior =
      widget_base_->LayerTreeHost()->overscroll_behavior();
  widget_base_->input_handler().DidOverscrollFromBlink(
      overscroll_delta, accumulated_overscroll, position, velocity,
      overscroll_behavior);
}

void WebExternalWidgetImpl::SetRootLayer(scoped_refptr<cc::Layer> layer) {
  widget_base_->LayerTreeHost()->SetNonBlinkManagedRootLayer(layer);
}

void WebExternalWidgetImpl::RequestNewLayerTreeFrameSink(
    LayerTreeFrameSinkCallback callback) {
  client_->RequestNewLayerTreeFrameSink(std::move(callback));
}

void WebExternalWidgetImpl::RecordTimeToFirstActivePaint(
    base::TimeDelta duration) {
  client_->RecordTimeToFirstActivePaint(duration);
}

void WebExternalWidgetImpl::DidCommitAndDrawCompositorFrame() {
  client_->DidCommitAndDrawCompositorFrame();
}

bool WebExternalWidgetImpl::WillHandleGestureEvent(
    const WebGestureEvent& event) {
  return client_->WillHandleGestureEvent(event);
}

bool WebExternalWidgetImpl::WillHandleMouseEvent(const WebMouseEvent& event) {
  return false;
}

void WebExternalWidgetImpl::ObserveGestureEventAndResult(
    const WebGestureEvent& gesture_event,
    const gfx::Vector2dF& unused_delta,
    const cc::OverscrollBehavior& overscroll_behavior,
    bool event_processed) {
  client_->DidHandleGestureScrollEvent(gesture_event, unused_delta,
                                       overscroll_behavior, event_processed);
}

bool WebExternalWidgetImpl::SupportsBufferedTouchEvents() {
  return client_->SupportsBufferedTouchEvents();
}

void WebExternalWidgetImpl::DidHandleKeyEvent() {}

void WebExternalWidgetImpl::QueueSyntheticEvent(
    std::unique_ptr<blink::WebCoalescedInputEvent>) {}

}  // namespace blink
