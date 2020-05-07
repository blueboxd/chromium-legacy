// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/web_frame_widget_base.h"

#include <memory>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/swap_promise.h"
#include "cc/trees/ukm_manager.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/web_render_widget_scheduling_state.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_widget_client.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/events/web_input_event_conversion.h"
#include "third_party/blink/renderer/core/events/wheel_event.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame_ukm_aggregator.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/input/context_menu_allowed_scope.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/hit_test_request.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/page/context_menu_controller.h"
#include "third_party/blink/renderer/core/page/drag_actions.h"
#include "third_party/blink/renderer/core/page/drag_controller.h"
#include "third_party/blink/renderer/core/page/drag_data.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/pointer_lock_controller.h"
#include "third_party/blink/renderer/platform/graphics/animation_worklet_mutator_dispatcher_impl.h"
#include "third_party/blink/renderer/platform/graphics/compositor_mutator_client.h"
#include "third_party/blink/renderer/platform/graphics/paint_worklet_paint_dispatcher.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/widget/widget_base.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace WTF {
template <>
struct CrossThreadCopier<blink::WebReportTimeCallback>
    : public CrossThreadCopierByValuePassThrough<blink::WebReportTimeCallback> {
  STATIC_ONLY(CrossThreadCopier);
};

}  // namespace WTF

namespace blink {

// Ensure that the WebDragOperation enum values stay in sync with the original
// DragOperation constants.
STATIC_ASSERT_ENUM(kDragOperationNone, kWebDragOperationNone);
STATIC_ASSERT_ENUM(kDragOperationCopy, kWebDragOperationCopy);
STATIC_ASSERT_ENUM(kDragOperationLink, kWebDragOperationLink);
STATIC_ASSERT_ENUM(kDragOperationGeneric, kWebDragOperationGeneric);
STATIC_ASSERT_ENUM(kDragOperationPrivate, kWebDragOperationPrivate);
STATIC_ASSERT_ENUM(kDragOperationMove, kWebDragOperationMove);
STATIC_ASSERT_ENUM(kDragOperationDelete, kWebDragOperationDelete);
STATIC_ASSERT_ENUM(kDragOperationEvery, kWebDragOperationEvery);

bool WebFrameWidgetBase::ignore_input_events_ = false;

WebFrameWidgetBase::WebFrameWidgetBase(
    WebWidgetClient& client,
    CrossVariantMojoAssociatedRemote<mojom::blink::FrameWidgetHostInterfaceBase>
        frame_widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::FrameWidgetInterfaceBase>
        frame_widget,
    CrossVariantMojoAssociatedRemote<mojom::blink::WidgetHostInterfaceBase>
        widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::blink::WidgetInterfaceBase>
        widget)
    : widget_base_(std::make_unique<WidgetBase>(this,
                                                std::move(widget_host),
                                                std::move(widget))),
      client_(&client),
      frame_widget_host_(std::move(frame_widget_host)),
      receiver_(this, std::move(frame_widget)) {}

WebFrameWidgetBase::~WebFrameWidgetBase() = default;

void WebFrameWidgetBase::BindLocalRoot(WebLocalFrame& local_root) {
  local_root_ = To<WebLocalFrameImpl>(local_root);
  local_root_->SetFrameWidget(this);
  request_animation_after_delay_timer_.reset(
      new TaskRunnerTimer<WebFrameWidgetBase>(
          local_root.GetTaskRunner(TaskType::kInternalDefault), this,
          &WebFrameWidgetBase::RequestAnimationAfterDelayTimerFired));
}

void WebFrameWidgetBase::Close(
    scoped_refptr<base::SingleThreadTaskRunner> cleanup_runner,
    base::OnceCallback<void()> cleanup_task) {
  mutator_dispatcher_ = nullptr;
  local_root_->SetFrameWidget(nullptr);
  local_root_ = nullptr;
  client_ = nullptr;
  request_animation_after_delay_timer_.reset();
  widget_base_->Shutdown(std::move(cleanup_runner), std::move(cleanup_task));
  widget_base_.reset();
  receiver_.reset();
}

WebLocalFrame* WebFrameWidgetBase::LocalRoot() const {
  return local_root_;
}

WebRect WebFrameWidgetBase::ComputeBlockBound(
    const gfx::Point& point_in_root_frame,
    bool ignore_clipping) const {
  HitTestLocation location(local_root_->GetFrameView()->ConvertFromRootFrame(
      PhysicalOffset(IntPoint(point_in_root_frame))));
  HitTestRequest::HitTestRequestType hit_type =
      HitTestRequest::kReadOnly | HitTestRequest::kActive |
      (ignore_clipping ? HitTestRequest::kIgnoreClipping : 0);
  HitTestResult result =
      local_root_->GetFrame()->GetEventHandler().HitTestResultAtLocation(
          location, hit_type);
  result.SetToShadowHostIfInRestrictedShadowRoot();

  Node* node = result.InnerNodeOrImageMapImage();
  if (!node)
    return WebRect();

  // Find the block type node based on the hit node.
  // FIXME: This wants to walk flat tree with
  // LayoutTreeBuilderTraversal::parent().
  while (node &&
         (!node->GetLayoutObject() || node->GetLayoutObject()->IsInline()))
    node = LayoutTreeBuilderTraversal::Parent(*node);

  // Return the bounding box in the root frame's coordinate space.
  if (node) {
    IntRect absolute_rect = node->GetLayoutObject()->AbsoluteBoundingBoxRect();
    LocalFrame* frame = node->GetDocument().GetFrame();
    return frame->View()->ConvertToRootFrame(absolute_rect);
  }
  return WebRect();
}

WebDragOperation WebFrameWidgetBase::DragTargetDragEnter(
    const WebDragData& web_drag_data,
    const gfx::PointF& point_in_viewport,
    const gfx::PointF& screen_point,
    WebDragOperationsMask operations_allowed,
    int modifiers) {
  DCHECK(!current_drag_data_);

  current_drag_data_ = DataObject::Create(web_drag_data);
  operations_allowed_ = operations_allowed;

  return DragTargetDragEnterOrOver(point_in_viewport, screen_point, kDragEnter,
                                   modifiers);
}

WebDragOperation WebFrameWidgetBase::DragTargetDragOver(
    const gfx::PointF& point_in_viewport,
    const gfx::PointF& screen_point,
    WebDragOperationsMask operations_allowed,
    int modifiers) {
  operations_allowed_ = operations_allowed;

  return DragTargetDragEnterOrOver(point_in_viewport, screen_point, kDragOver,
                                   modifiers);
}

void WebFrameWidgetBase::DragTargetDragLeave(
    const gfx::PointF& point_in_viewport,
    const gfx::PointF& screen_point) {
  DCHECK(current_drag_data_);

  // TODO(paulmeyer): It shouldn't be possible for |m_currentDragData| to be
  // null here, but this is somehow happening (rarely). This suggests that in
  // some cases drag-leave is happening before drag-enter, which should be
  // impossible. This needs to be investigated further. Once fixed, the extra
  // check for |!m_currentDragData| should be removed. (crbug.com/671152)
  if (IgnoreInputEvents() || !current_drag_data_) {
    CancelDrag();
    return;
  }

  gfx::PointF point_in_root_frame(ViewportToRootFrame(point_in_viewport));
  DragData drag_data(current_drag_data_.Get(), FloatPoint(point_in_root_frame),
                     FloatPoint(screen_point),
                     static_cast<DragOperation>(operations_allowed_));

  GetPage()->GetDragController().DragExited(&drag_data,
                                            *local_root_->GetFrame());

  // FIXME: why is the drag scroll timer not stopped here?

  drag_operation_ = kWebDragOperationNone;
  current_drag_data_ = nullptr;
}

void WebFrameWidgetBase::DragTargetDrop(const WebDragData& web_drag_data,
                                        const gfx::PointF& point_in_viewport,
                                        const gfx::PointF& screen_point,
                                        int modifiers) {
  gfx::PointF point_in_root_frame(ViewportToRootFrame(point_in_viewport));

  DCHECK(current_drag_data_);
  current_drag_data_ = DataObject::Create(web_drag_data);

  // If this webview transitions from the "drop accepting" state to the "not
  // accepting" state, then our IPC message reply indicating that may be in-
  // flight, or else delayed by javascript processing in this webview.  If a
  // drop happens before our IPC reply has reached the browser process, then
  // the browser forwards the drop to this webview.  So only allow a drop to
  // proceed if our webview m_dragOperation state is not DragOperationNone.

  if (drag_operation_ == kWebDragOperationNone) {
    // IPC RACE CONDITION: do not allow this drop.
    DragTargetDragLeave(point_in_viewport, screen_point);
    return;
  }

  if (!IgnoreInputEvents()) {
    current_drag_data_->SetModifiers(modifiers);
    DragData drag_data(current_drag_data_.Get(),
                       FloatPoint(point_in_root_frame),
                       FloatPoint(screen_point),
                       static_cast<DragOperation>(operations_allowed_));

    GetPage()->GetDragController().PerformDrag(&drag_data,
                                               *local_root_->GetFrame());
  }
  drag_operation_ = kWebDragOperationNone;
  current_drag_data_ = nullptr;
}

void WebFrameWidgetBase::DragSourceEndedAt(const gfx::PointF& point_in_viewport,
                                           const gfx::PointF& screen_point,
                                           WebDragOperation operation) {
  if (!local_root_) {
    // We should figure out why |local_root_| could be nullptr
    // (https://crbug.com/792345).
    return;
  }

  if (IgnoreInputEvents()) {
    CancelDrag();
    return;
  }
  gfx::PointF point_in_root_frame(
      GetPage()->GetVisualViewport().ViewportToRootFrame(
          FloatPoint(point_in_viewport)));

  WebMouseEvent fake_mouse_move(
      WebInputEvent::Type::kMouseMove, point_in_root_frame, screen_point,
      WebPointerProperties::Button::kLeft, 0, WebInputEvent::kNoModifiers,
      base::TimeTicks::Now());
  fake_mouse_move.SetFrameScale(1);
  local_root_->GetFrame()->GetEventHandler().DragSourceEndedAt(
      fake_mouse_move, static_cast<DragOperation>(operation));
}

void WebFrameWidgetBase::DragSourceSystemDragEnded() {
  CancelDrag();
}

void WebFrameWidgetBase::SetBackgroundOpaque(bool opaque) {
  if (opaque) {
    View()->ClearBaseBackgroundColorOverride();
    View()->ClearBackgroundColorOverride();
  } else {
    View()->SetBaseBackgroundColorOverride(SK_ColorTRANSPARENT);
    View()->SetBackgroundColorOverride(SK_ColorTRANSPARENT);
  }
}

void WebFrameWidgetBase::CancelDrag() {
  // It's possible for this to be called while we're not doing a drag if
  // it's from a previous page that got unloaded.
  if (!doing_drag_and_drop_)
    return;
  GetPage()->GetDragController().DragEnded();
  doing_drag_and_drop_ = false;
}

void WebFrameWidgetBase::StartDragging(network::mojom::ReferrerPolicy policy,
                                       const WebDragData& data,
                                       WebDragOperationsMask mask,
                                       const SkBitmap& drag_image,
                                       const gfx::Point& drag_image_offset) {
  doing_drag_and_drop_ = true;
  Client()->StartDragging(policy, data, mask, drag_image, drag_image_offset);
}

WebDragOperation WebFrameWidgetBase::DragTargetDragEnterOrOver(
    const gfx::PointF& point_in_viewport,
    const gfx::PointF& screen_point,
    DragAction drag_action,
    int modifiers) {
  DCHECK(current_drag_data_);
  // TODO(paulmeyer): It shouldn't be possible for |m_currentDragData| to be
  // null here, but this is somehow happening (rarely). This suggests that in
  // some cases drag-over is happening before drag-enter, which should be
  // impossible. This needs to be investigated further. Once fixed, the extra
  // check for |!m_currentDragData| should be removed. (crbug.com/671504)
  if (IgnoreInputEvents() || !current_drag_data_) {
    CancelDrag();
    return kWebDragOperationNone;
  }

  FloatPoint point_in_root_frame(ViewportToRootFrame(point_in_viewport));

  current_drag_data_->SetModifiers(modifiers);
  DragData drag_data(current_drag_data_.Get(), FloatPoint(point_in_root_frame),
                     FloatPoint(screen_point),
                     static_cast<DragOperation>(operations_allowed_));

  DragOperation drag_operation =
      GetPage()->GetDragController().DragEnteredOrUpdated(
          &drag_data, *local_root_->GetFrame());

  // Mask the drag operation against the drag source's allowed
  // operations.
  if (!(drag_operation & drag_data.DraggingSourceOperationMask()))
    drag_operation = kDragOperationNone;

  drag_operation_ = static_cast<WebDragOperation>(drag_operation);

  return drag_operation_;
}

void WebFrameWidgetBase::SendOverscrollEventFromImplSide(
    const gfx::Vector2dF& overscroll_delta,
    cc::ElementId scroll_latched_element_id) {
  if (!RuntimeEnabledFeatures::OverscrollCustomizationEnabled())
    return;

  Node* target_node = View()->FindNodeFromScrollableCompositorElementId(
      scroll_latched_element_id);
  if (target_node) {
    target_node->GetDocument().EnqueueOverscrollEventForNode(
        target_node, overscroll_delta.x(), overscroll_delta.y());
  }
}

void WebFrameWidgetBase::SendScrollEndEventFromImplSide(
    cc::ElementId scroll_latched_element_id) {
  if (!RuntimeEnabledFeatures::OverscrollCustomizationEnabled())
    return;

  Node* target_node = View()->FindNodeFromScrollableCompositorElementId(
      scroll_latched_element_id);
  if (target_node)
    target_node->GetDocument().EnqueueScrollEndEventForNode(target_node);
}

gfx::PointF WebFrameWidgetBase::ViewportToRootFrame(
    const gfx::PointF& point_in_viewport) const {
  return GetPage()->GetVisualViewport().ViewportToRootFrame(
      FloatPoint(point_in_viewport));
}

WebViewImpl* WebFrameWidgetBase::View() const {
  return local_root_->ViewImpl();
}

Page* WebFrameWidgetBase::GetPage() const {
  return View()->GetPage();
}

const mojo::AssociatedRemote<mojom::blink::FrameWidgetHost>&
WebFrameWidgetBase::GetAssociatedFrameWidgetHost() const {
  return frame_widget_host_;
}

void WebFrameWidgetBase::DidAcquirePointerLock() {
  GetPage()->GetPointerLockController().DidAcquirePointerLock();

  LocalFrame* focusedFrame = FocusedLocalFrameInWidget();
  if (focusedFrame) {
    focusedFrame->GetEventHandler().ReleaseMousePointerCapture();
  }
}

void WebFrameWidgetBase::DidNotAcquirePointerLock() {
  GetPage()->GetPointerLockController().DidNotAcquirePointerLock();
}

void WebFrameWidgetBase::DidLosePointerLock() {
  GetPage()->GetPointerLockController().DidLosePointerLock();
}

void WebFrameWidgetBase::RequestDecode(
    const PaintImage& image,
    base::OnceCallback<void(bool)> callback) {
  Client()->RequestDecode(image, std::move(callback));
}

void WebFrameWidgetBase::Trace(Visitor* visitor) {
  visitor->Trace(local_root_);
  visitor->Trace(current_drag_data_);
}

void WebFrameWidgetBase::SetNeedsRecalculateRasterScales() {
  if (!View()->does_composite())
    return;
  widget_base_->LayerTreeHost()->SetNeedsRecalculateRasterScales();
}

void WebFrameWidgetBase::SetBackgroundColor(SkColor color) {
  if (!View()->does_composite())
    return;
  widget_base_->LayerTreeHost()->set_background_color(color);
}

void WebFrameWidgetBase::SetOverscrollBehavior(
    const cc::OverscrollBehavior& overscroll_behavior) {
  if (!View()->does_composite())
    return;
  widget_base_->LayerTreeHost()->SetOverscrollBehavior(overscroll_behavior);
}

void WebFrameWidgetBase::RegisterSelection(cc::LayerSelection selection) {
  if (!View()->does_composite())
    return;
  widget_base_->LayerTreeHost()->RegisterSelection(selection);
}

void WebFrameWidgetBase::StartPageScaleAnimation(
    const gfx::Vector2d& destination,
    bool use_anchor,
    float new_page_scale,
    base::TimeDelta duration) {
  widget_base_->LayerTreeHost()->StartPageScaleAnimation(
      destination, use_anchor, new_page_scale, duration);
}

void WebFrameWidgetBase::RequestBeginMainFrameNotExpected(bool request) {
  if (!View()->does_composite())
    return;
  widget_base_->LayerTreeHost()->RequestBeginMainFrameNotExpected(request);
}

void WebFrameWidgetBase::EndCommitCompositorFrame(
    base::TimeTicks commit_start_time) {
  Client()->DidCommitCompositorFrame(commit_start_time);
}

void WebFrameWidgetBase::DidCommitAndDrawCompositorFrame() {
  Client()->DidCommitAndDrawCompositorFrame();
}

void WebFrameWidgetBase::OnDeferMainFrameUpdatesChanged(bool defer) {
  Client()->OnDeferMainFrameUpdatesChanged(defer);
}

void WebFrameWidgetBase::OnDeferCommitsChanged(bool defer) {
  Client()->OnDeferCommitsChanged(defer);
}

void WebFrameWidgetBase::RequestNewLayerTreeFrameSink(
    LayerTreeFrameSinkCallback callback) {
  Client()->RequestNewLayerTreeFrameSink(std::move(callback));
}

void WebFrameWidgetBase::DidCompletePageScaleAnimation() {
  Client()->DidCompletePageScaleAnimation();
}

void WebFrameWidgetBase::DidBeginMainFrame() {
  Client()->DidBeginMainFrame();
}

void WebFrameWidgetBase::WillBeginMainFrame() {
  Client()->WillBeginMainFrame();
}

int WebFrameWidgetBase::GetLayerTreeId() {
  if (!View()->does_composite())
    return 0;
  return widget_base_->LayerTreeHost()->GetId();
}

void WebFrameWidgetBase::SetHaveScrollEventHandlers(bool has_handlers) {
  widget_base_->LayerTreeHost()->SetHaveScrollEventHandlers(has_handlers);
}

void WebFrameWidgetBase::SetEventListenerProperties(
    cc::EventListenerClass listener_class,
    cc::EventListenerProperties listener_properties) {
  widget_base_->LayerTreeHost()->SetEventListenerProperties(
      listener_class, listener_properties);

  if (listener_class == cc::EventListenerClass::kTouchStartOrMove ||
      listener_class == cc::EventListenerClass::kTouchEndOrCancel) {
    bool has_touch_handlers =
        EventListenerProperties(cc::EventListenerClass::kTouchStartOrMove) !=
            cc::EventListenerProperties::kNone ||
        EventListenerProperties(cc::EventListenerClass::kTouchEndOrCancel) !=
            cc::EventListenerProperties::kNone;
    if (!has_touch_handlers_ || *has_touch_handlers_ != has_touch_handlers) {
      has_touch_handlers_ = has_touch_handlers;

      // Can be NULL when running tests.
      if (auto* scheduler_state = widget_base_->RendererWidgetSchedulingState())
        scheduler_state->SetHasTouchHandler(has_touch_handlers);
      frame_widget_host_->SetHasTouchEventHandlers(has_touch_handlers);
    }
  } else if (listener_class == cc::EventListenerClass::kPointerRawUpdate) {
    client_->SetHasPointerRawUpdateEventHandlers(
        listener_properties != cc::EventListenerProperties::kNone);
  }
}

cc::EventListenerProperties WebFrameWidgetBase::EventListenerProperties(
    cc::EventListenerClass listener_class) const {
  return widget_base_->LayerTreeHost()->event_listener_properties(
      listener_class);
}

mojom::blink::DisplayMode WebFrameWidgetBase::DisplayMode() const {
  return display_mode_;
}

void WebFrameWidgetBase::StartDeferringCommits(base::TimeDelta timeout) {
  if (!View()->does_composite())
    return;
  widget_base_->LayerTreeHost()->StartDeferringCommits(timeout);
}

void WebFrameWidgetBase::StopDeferringCommits(
    cc::PaintHoldingCommitTrigger triggger) {
  if (!View()->does_composite())
    return;
  widget_base_->LayerTreeHost()->StopDeferringCommits(triggger);
}

std::unique_ptr<cc::ScopedDeferMainFrameUpdate>
WebFrameWidgetBase::DeferMainFrameUpdate() {
  return widget_base_->LayerTreeHost()->DeferMainFrameUpdate();
}

void WebFrameWidgetBase::SetBrowserControlsShownRatio(float top_ratio,
                                                      float bottom_ratio) {
  widget_base_->LayerTreeHost()->SetBrowserControlsShownRatio(top_ratio,
                                                              bottom_ratio);
}

void WebFrameWidgetBase::SetBrowserControlsParams(
    cc::BrowserControlsParams params) {
  widget_base_->LayerTreeHost()->SetBrowserControlsParams(params);
}

cc::LayerTreeDebugState WebFrameWidgetBase::GetLayerTreeDebugState() {
  return widget_base_->LayerTreeHost()->GetDebugState();
}

void WebFrameWidgetBase::SetLayerTreeDebugState(
    const cc::LayerTreeDebugState& state) {
  widget_base_->LayerTreeHost()->SetDebugState(state);
}

void WebFrameWidgetBase::SynchronouslyCompositeForTesting(
    base::TimeTicks frame_time) {
  widget_base_->LayerTreeHost()->Composite(frame_time, false);
}

// TODO(665924): Remove direct dispatches of mouse events from
// PointerLockController, instead passing them through EventHandler.
void WebFrameWidgetBase::PointerLockMouseEvent(
    const WebCoalescedInputEvent& coalesced_event) {
  const WebInputEvent& input_event = coalesced_event.Event();
  const WebMouseEvent& mouse_event =
      static_cast<const WebMouseEvent&>(input_event);
  WebMouseEvent transformed_event =
      TransformWebMouseEvent(local_root_->GetFrameView(), mouse_event);

  AtomicString event_type;
  switch (input_event.GetType()) {
    case WebInputEvent::Type::kMouseDown:
      event_type = event_type_names::kMousedown;
      if (!GetPage() || !GetPage()->GetPointerLockController().GetElement())
        break;
      LocalFrame::NotifyUserActivation(GetPage()
                                           ->GetPointerLockController()
                                           .GetElement()
                                           ->GetDocument()
                                           .GetFrame());
      break;
    case WebInputEvent::Type::kMouseUp:
      event_type = event_type_names::kMouseup;
      break;
    case WebInputEvent::Type::kMouseMove:
      event_type = event_type_names::kMousemove;
      break;
    default:
      NOTREACHED() << input_event.GetType();
  }

  if (GetPage()) {
    GetPage()->GetPointerLockController().DispatchLockedMouseEvent(
        transformed_event,
        TransformWebMouseEventVector(
            local_root_->GetFrameView(),
            coalesced_event.GetCoalescedEventsPointers()),
        TransformWebMouseEventVector(
            local_root_->GetFrameView(),
            coalesced_event.GetPredictedEventsPointers()),
        event_type);
  }
}

void WebFrameWidgetBase::ShowContextMenu(WebMenuSourceType source_type) {
  if (!GetPage())
    return;

  GetPage()->GetContextMenuController().ClearContextMenu();
  {
    ContextMenuAllowedScope scope;
    if (LocalFrame* focused_frame =
            GetPage()->GetFocusController().FocusedFrame()) {
      focused_frame->GetEventHandler().ShowNonLocatedContextMenu(nullptr,
                                                                 source_type);
    }
  }
}

LocalFrame* WebFrameWidgetBase::FocusedLocalFrameInWidget() const {
  if (!local_root_) {
    // WebFrameWidget is created in the call to CreateFrame. The corresponding
    // RenderWidget, however, might not swap in right away (InstallNewDocument()
    // will lead to it swapping in). During this interval local_root_ is nullptr
    // (see https://crbug.com/792345).
    return nullptr;
  }

  LocalFrame* frame = GetPage()->GetFocusController().FocusedFrame();
  return (frame && frame->LocalFrameRoot() == local_root_->GetFrame())
             ? frame
             : nullptr;
}

WebLocalFrame* WebFrameWidgetBase::FocusedWebLocalFrameInWidget() const {
  return WebLocalFrameImpl::FromFrame(FocusedLocalFrameInWidget());
}

cc::LayerTreeHost* WebFrameWidgetBase::InitializeCompositing(
    cc::TaskGraphRunner* task_graph_runner,
    const cc::LayerTreeSettings& settings,
    std::unique_ptr<cc::UkmRecorderFactory> ukm_recorder_factory) {
  widget_base_->InitializeCompositing(task_graph_runner, settings,
                                      std::move(ukm_recorder_factory));
  GetPage()->AnimationHostInitialized(*AnimationHost(),
                                      GetLocalFrameViewForAnimationScrolling());
  return widget_base_->LayerTreeHost();
}

void WebFrameWidgetBase::SetCompositorVisible(bool visible) {
  widget_base_->SetCompositorVisible(visible);
}

void WebFrameWidgetBase::RecordTimeToFirstActivePaint(
    base::TimeDelta duration) {
  Client()->RecordTimeToFirstActivePaint(duration);
}

void WebFrameWidgetBase::DispatchRafAlignedInput(base::TimeTicks frame_time) {
  base::TimeTicks raf_aligned_input_start_time;
  if (LocalRootImpl() && WidgetBase::ShouldRecordBeginMainFrameMetrics()) {
    raf_aligned_input_start_time = base::TimeTicks::Now();
  }

  Client()->DispatchRafAlignedInput(frame_time);

  if (LocalRootImpl() && WidgetBase::ShouldRecordBeginMainFrameMetrics()) {
    LocalRootImpl()->GetFrame()->View()->EnsureUkmAggregator().RecordSample(
        LocalFrameUkmAggregator::kHandleInputEvents,
        raf_aligned_input_start_time, base::TimeTicks::Now());
  }
}

void WebFrameWidgetBase::ApplyViewportChangesForTesting(
    const ApplyViewportChangesArgs& args) {
  widget_base_->ApplyViewportChanges(args);
}

void WebFrameWidgetBase::SetDisplayMode(mojom::blink::DisplayMode mode) {
  if (mode != display_mode_) {
    display_mode_ = mode;
    LocalFrame* frame = LocalRootImpl()->GetFrame();
    frame->MediaQueryAffectingValueChangedForLocalSubtree(
        MediaValueChange::kOther);
  }
}

void WebFrameWidgetBase::RequestAnimationAfterDelay(
    const base::TimeDelta& delay) {
  DCHECK(request_animation_after_delay_timer_.get());
  if (request_animation_after_delay_timer_->IsActive() &&
      request_animation_after_delay_timer_->NextFireInterval() > delay) {
    request_animation_after_delay_timer_->Stop();
  }
  if (!request_animation_after_delay_timer_->IsActive()) {
    request_animation_after_delay_timer_->StartOneShot(delay, FROM_HERE);
  }
}

void WebFrameWidgetBase::RequestAnimationAfterDelayTimerFired(TimerBase*) {
  if (client_)
    client_->ScheduleAnimation();
}

base::WeakPtr<AnimationWorkletMutatorDispatcherImpl>
WebFrameWidgetBase::EnsureCompositorMutatorDispatcher(
    scoped_refptr<base::SingleThreadTaskRunner>* mutator_task_runner) {
  if (!mutator_task_runner_) {
    widget_base_->LayerTreeHost()->SetLayerTreeMutator(
        AnimationWorkletMutatorDispatcherImpl::CreateCompositorThreadClient(
            &mutator_dispatcher_, &mutator_task_runner_));
  }

  DCHECK(mutator_task_runner_);
  *mutator_task_runner = mutator_task_runner_;
  return mutator_dispatcher_;
}

cc::AnimationHost* WebFrameWidgetBase::AnimationHost() const {
  return widget_base_->AnimationHost();
}

base::WeakPtr<PaintWorkletPaintDispatcher>
WebFrameWidgetBase::EnsureCompositorPaintDispatcher(
    scoped_refptr<base::SingleThreadTaskRunner>* paint_task_runner) {
  // We check paint_task_runner_ not paint_dispatcher_ because the dispatcher is
  // a base::WeakPtr that should only be used on the compositor thread.
  if (!paint_task_runner_) {
    widget_base_->LayerTreeHost()->SetPaintWorkletLayerPainter(
        PaintWorkletPaintDispatcher::CreateCompositorThreadPainter(
            &paint_dispatcher_));
    paint_task_runner_ = Thread::CompositorThread()->GetTaskRunner();
  }
  DCHECK(paint_task_runner_);
  *paint_task_runner = paint_task_runner_;
  return paint_dispatcher_;
}

// Enables measuring and reporting both presentation times and swap times in
// swap promises.
class ReportTimeSwapPromise : public cc::SwapPromise {
 public:
  ReportTimeSwapPromise(WebReportTimeCallback swap_time_callback,
                        WebReportTimeCallback presentation_time_callback,
                        scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                        WebFrameWidgetBase* widget)
      : swap_time_callback_(std::move(swap_time_callback)),
        presentation_time_callback_(std::move(presentation_time_callback)),
        task_runner_(std::move(task_runner)),
        widget_(widget) {}
  ~ReportTimeSwapPromise() override = default;

  void DidActivate() override {}

  void WillSwap(viz::CompositorFrameMetadata* metadata) override {
    DCHECK_GT(metadata->frame_token, 0u);
    // The interval between the current swap and its presentation time is
    // reported in UMA (see corresponding code in DidSwap() below).
    frame_token_ = metadata->frame_token;
  }

  void DidSwap() override {
    DCHECK_GT(frame_token_, 0u);
    PostCrossThreadTask(
        *task_runner_, FROM_HERE,
        CrossThreadBindOnce(
            &RunCallbackAfterSwap, widget_, base::TimeTicks::Now(),
            std::move(swap_time_callback_),
            std::move(presentation_time_callback_), frame_token_));
  }

  cc::SwapPromise::DidNotSwapAction DidNotSwap(
      DidNotSwapReason reason) override {
    WebSwapResult result;
    switch (reason) {
      case cc::SwapPromise::DidNotSwapReason::SWAP_FAILS:
        result = WebSwapResult::kDidNotSwapSwapFails;
        break;
      case cc::SwapPromise::DidNotSwapReason::COMMIT_FAILS:
        result = WebSwapResult::kDidNotSwapCommitFails;
        break;
      case cc::SwapPromise::DidNotSwapReason::COMMIT_NO_UPDATE:
        result = WebSwapResult::kDidNotSwapCommitNoUpdate;
        break;
      case cc::SwapPromise::DidNotSwapReason::ACTIVATION_FAILS:
        result = WebSwapResult::kDidNotSwapActivationFails;
        break;
    }
    // During a failed swap, return the current time regardless of whether we're
    // using presentation or swap timestamps.
    PostCrossThreadTask(
        *task_runner_, FROM_HERE,
        CrossThreadBindOnce(
            [](WebSwapResult result, base::TimeTicks swap_time,
               WebReportTimeCallback swap_time_callback,
               WebReportTimeCallback presentation_time_callback) {
              ReportTime(std::move(swap_time_callback), result, swap_time);
              ReportTime(std::move(presentation_time_callback), result,
                         swap_time);
            },
            result, base::TimeTicks::Now(), std::move(swap_time_callback_),
            std::move(presentation_time_callback_)));
    return DidNotSwapAction::BREAK_PROMISE;
  }

  int64_t TraceId() const override { return 0; }

 private:
  static void RunCallbackAfterSwap(
      WebFrameWidgetBase* widget,
      base::TimeTicks swap_time,
      WebReportTimeCallback swap_time_callback,
      WebReportTimeCallback presentation_time_callback,
      int frame_token) {
    // If the widget was collected or the widget wasn't collected yet, but
    // it was closed don't schedule a presentation callback.
    if (widget && widget->widget_base_) {
      widget->widget_base_->AddPresentationCallback(
          frame_token,
          WTF::Bind(&RunCallbackAfterPresentation,
                    std::move(presentation_time_callback), swap_time));
      ReportTime(std::move(swap_time_callback), WebSwapResult::kDidSwap,
                 swap_time);
    } else {
      ReportTime(std::move(swap_time_callback), WebSwapResult::kDidSwap,
                 swap_time);
      ReportTime(std::move(presentation_time_callback), WebSwapResult::kDidSwap,
                 swap_time);
    }
  }

  static void RunCallbackAfterPresentation(
      WebReportTimeCallback presentation_time_callback,
      base::TimeTicks swap_time,
      base::TimeTicks presentation_time) {
    DCHECK(!swap_time.is_null());
    bool presentation_time_is_valid =
        !presentation_time.is_null() && (presentation_time > swap_time);
    UMA_HISTOGRAM_BOOLEAN("PageLoad.Internal.Renderer.PresentationTime.Valid",
                          presentation_time_is_valid);
    if (presentation_time_is_valid) {
      // This measures from 1ms to 10seconds.
      UMA_HISTOGRAM_TIMES(
          "PageLoad.Internal.Renderer.PresentationTime.DeltaFromSwapTime",
          presentation_time - swap_time);
    }
    ReportTime(std::move(presentation_time_callback), WebSwapResult::kDidSwap,
               presentation_time_is_valid ? presentation_time : swap_time);
  }

  static void ReportTime(WebReportTimeCallback callback,
                         WebSwapResult result,
                         base::TimeTicks time) {
    if (callback)
      std::move(callback).Run(result, time);
  }

  WebReportTimeCallback swap_time_callback_;
  WebReportTimeCallback presentation_time_callback_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  CrossThreadWeakPersistent<WebFrameWidgetBase> widget_;
  uint32_t frame_token_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ReportTimeSwapPromise);
};

void WebFrameWidgetBase::NotifySwapAndPresentationTimeInBlink(
    WebReportTimeCallback swap_time_callback,
    WebReportTimeCallback presentation_time_callback) {
  NotifySwapAndPresentationTime(std::move(swap_time_callback),
                                std::move(presentation_time_callback));
}

void WebFrameWidgetBase::NotifySwapAndPresentationTime(
    WebReportTimeCallback swap_time_callback,
    WebReportTimeCallback presentation_time_callback) {
  if (!View()->does_composite())
    return;
  widget_base_->LayerTreeHost()->QueueSwapPromise(
      std::make_unique<ReportTimeSwapPromise>(
          std::move(swap_time_callback), std::move(presentation_time_callback),
          widget_base_->LayerTreeHost()
              ->GetTaskRunnerProvider()
              ->MainThreadTaskRunner(),
          this));
}

scheduler::WebRenderWidgetSchedulingState*
WebFrameWidgetBase::RendererWidgetSchedulingState() {
  return widget_base_->RendererWidgetSchedulingState();
}

}  // namespace blink
