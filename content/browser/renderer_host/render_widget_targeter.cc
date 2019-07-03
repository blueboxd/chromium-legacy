// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_targeter.h"

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "components/viz/common/features.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/renderer_host/input/one_shot_timeout_monitor.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/public/browser/site_isolation_policy.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "ui/events/blink/blink_event_util.h"

namespace content {

namespace {

bool MergeEventIfPossible(const blink::WebInputEvent& event,
                          ui::WebScopedInputEvent* blink_event) {
  if (!blink::WebInputEvent::IsTouchEventType(event.GetType()) &&
      !blink::WebInputEvent::IsGestureEventType(event.GetType()) &&
      ui::CanCoalesce(event, **blink_event)) {
    ui::Coalesce(event, blink_event->get());
    return true;
  }
  return false;
}

gfx::PointF ComputeEventLocation(const blink::WebInputEvent& event) {
  if (blink::WebInputEvent::IsMouseEventType(event.GetType()) ||
      event.GetType() == blink::WebInputEvent::kMouseWheel) {
    return static_cast<const blink::WebMouseEvent&>(event).PositionInWidget();
  }
  if (blink::WebInputEvent::IsTouchEventType(event.GetType())) {
    return static_cast<const blink::WebTouchEvent&>(event)
        .touches[0]
        .PositionInWidget();
  }
  if (blink::WebInputEvent::IsGestureEventType(event.GetType()))
    return static_cast<const blink::WebGestureEvent&>(event).PositionInWidget();

  return gfx::PointF();
}

constexpr const char kTracingCategory[] = "input,latency";

}  // namespace

class TracingUmaTracker {
 public:
  explicit TracingUmaTracker(const char* metric_name)
      : id_(next_id_++),
        start_time_(base::TimeTicks::Now()),
        metric_name_(metric_name) {
    TRACE_EVENT_ASYNC_BEGIN0(
        kTracingCategory, metric_name_,
        TRACE_ID_WITH_SCOPE("UmaTracker", TRACE_ID_LOCAL(id_)));
  }
  ~TracingUmaTracker() = default;
  TracingUmaTracker(TracingUmaTracker&& tracker) = default;

  void StopAndRecord() {
    Stop();
    UmaHistogramTimes(metric_name_, base::TimeTicks::Now() - start_time_);
  }

  void Stop() {
    TRACE_EVENT_ASYNC_END0(
        kTracingCategory, metric_name_,
        TRACE_ID_WITH_SCOPE("UmaTracker", TRACE_ID_LOCAL(id_)));
  }

 private:
  const int id_;
  const base::TimeTicks start_time_;

  // These variables must be string literals and live for the duration
  // of the program since tracing stores pointers.
  const char* metric_name_;

  static int next_id_;

  DISALLOW_COPY_AND_ASSIGN(TracingUmaTracker);
};

int TracingUmaTracker::next_id_ = 1;

enum class HitTestResultsMatch {
  kDoNotMatch = 0,
  kMatch = 1,
  kHitTestResultChanged = 2,
  kMaxValue = kHitTestResultChanged,
};

RenderWidgetTargetResult::RenderWidgetTargetResult() = default;

RenderWidgetTargetResult::RenderWidgetTargetResult(
    const RenderWidgetTargetResult&) = default;

RenderWidgetTargetResult::RenderWidgetTargetResult(
    RenderWidgetHostViewBase* in_view,
    bool in_should_query_view,
    base::Optional<gfx::PointF> in_location,
    bool in_latched_target,
    bool in_should_verify_result)
    : view(in_view),
      should_query_view(in_should_query_view),
      target_location(in_location),
      latched_target(in_latched_target),
      should_verify_result(in_should_verify_result) {}

RenderWidgetTargetResult::~RenderWidgetTargetResult() = default;

RenderWidgetTargeter::TargetingRequest::TargetingRequest() = default;

RenderWidgetTargeter::TargetingRequest::TargetingRequest(
    TargetingRequest&& request) = default;

RenderWidgetTargeter::TargetingRequest& RenderWidgetTargeter::TargetingRequest::
operator=(TargetingRequest&&) = default;

RenderWidgetTargeter::TargetingRequest::~TargetingRequest() = default;

RenderWidgetTargeter::RenderWidgetTargeter(Delegate* delegate)
    : trace_id_(base::RandUint64()),
      is_viz_hit_testing_debug_enabled_(
          features::IsVizHitTestingDebugEnabled()),
      delegate_(delegate),
      weak_ptr_factory_(this) {
  DCHECK(delegate_);
}

RenderWidgetTargeter::~RenderWidgetTargeter() = default;

void RenderWidgetTargeter::FindTargetAndDispatch(
    RenderWidgetHostViewBase* root_view,
    const blink::WebInputEvent& event,
    const ui::LatencyInfo& latency) {
  DCHECK(blink::WebInputEvent::IsMouseEventType(event.GetType()) ||
         event.GetType() == blink::WebInputEvent::kMouseWheel ||
         blink::WebInputEvent::IsTouchEventType(event.GetType()) ||
         (blink::WebInputEvent::IsGestureEventType(event.GetType()) &&
          (static_cast<const blink::WebGestureEvent&>(event).SourceDevice() ==
               blink::WebGestureDevice::kTouchscreen ||
           static_cast<const blink::WebGestureEvent&>(event).SourceDevice() ==
               blink::WebGestureDevice::kTouchpad)));

  if (request_in_flight_) {
    if (!requests_.empty()) {
      auto& request = requests_.back();
      if (MergeEventIfPossible(event, &request.event))
        return;
    }
    TargetingRequest request;
    request.root_view = root_view->GetWeakPtr();
    request.event = ui::WebInputEventTraits::Clone(event);
    request.latency = latency;
    request.tracker =
        std::make_unique<TracingUmaTracker>("Event.AsyncTargeting.TimeInQueue");
    requests_.push(std::move(request));
    return;
  }

  RenderWidgetTargetResult result =
      delegate_->FindTargetSynchronously(root_view, event);

  const gfx::PointF event_location = ComputeEventLocation(event);

  RenderWidgetHostViewBase* target = result.view;
  auto* event_ptr = &event;
  async_depth_ = 0;
  if (result.should_query_view) {
    TRACE_EVENT_WITH_FLOW2(
        "viz,benchmark", "Event.Pipeline", TRACE_ID_GLOBAL(trace_id_),
        TRACE_EVENT_FLAG_FLOW_OUT, "step", "QueryClient(Start)",
        "event_location", event_location.ToString());

    // TODO(kenrb, sadrul): When all event types support asynchronous hit
    // testing, we should be able to have FindTargetSynchronously return the
    // view and location to use for the renderer hit test query.
    // Currently it has to return the surface hit test target, for event types
    // that ignore |result.should_query_view|, and therefore we have to use
    // root_view and the original event location for the initial query.
    // Do not compare hit test results if we are forced to do async hit testing
    // by HitTestQuery.
    QueryClient(root_view, root_view, *event_ptr, latency, event_location,
                nullptr, gfx::PointF());
  } else {
    FoundTarget(root_view, target, *event_ptr, latency, result.target_location,
                result.latched_target, viz::FrameSinkId());
    // Verify the event targeting results from surface layer viz hit testing if
    // --use-viz-hit-test-surface-layer is enabled.
    if (result.should_verify_result && !target->IsRenderWidgetHostViewGuest()) {
      QueryAndVerifyClient(root_view, root_view, *event_ptr, latency,
                           event_location, nullptr, gfx::PointF(),
                           target->GetFrameSinkId());
    }
  }
}

void RenderWidgetTargeter::ViewWillBeDestroyed(RenderWidgetHostViewBase* view) {
  unresponsive_views_.erase(view);
}

bool RenderWidgetTargeter::HasEventsPendingDispatch() const {
  return request_in_flight_ || !requests_.empty();
}

void RenderWidgetTargeter::QueryClientInternal(
    RenderWidgetHostViewBase* root_view,
    RenderWidgetHostViewBase* target,
    const blink::WebInputEvent& event,
    const ui::LatencyInfo& latency,
    const gfx::PointF& target_location,
    RenderWidgetHostViewBase* last_request_target,
    const gfx::PointF& last_target_location,
    const viz::FrameSinkId& expected_frame_sink_id) {
  // Async event targeting and verifying use two different queues, so they don't
  // block each other.
  bool is_verifying = expected_frame_sink_id.is_valid();
  DCHECK((!is_verifying && !request_in_flight_) ||
         (is_verifying && !verify_request_in_flight_));

  auto* target_client = target->host()->input_target_client();
  // |target_client| may not be set yet for this |target| on Mac, need to
  // understand why this happens. https://crbug.com/859492.
  // We do not verify hit testing result under this circumstance.
  if (!target_client) {
    FoundTarget(root_view, target, event, latency, target_location, false,
                viz::FrameSinkId());
    return;
  }

  if (is_verifying) {
    verify_request_in_flight_ = true;
  } else {
    request_in_flight_ = true;
    async_depth_++;
  }
  TracingUmaTracker tracker("Event.AsyncTargeting.ResponseTime");
  auto& hit_test_timeout =
      is_verifying ? async_verify_hit_test_timeout_ : async_hit_test_timeout_;
  hit_test_timeout.reset(new OneShotTimeoutMonitor(
      base::BindOnce(
          &RenderWidgetTargeter::AsyncHitTestTimedOut,
          weak_ptr_factory_.GetWeakPtr(), root_view->GetWeakPtr(),
          target->GetWeakPtr(), target_location,
          last_request_target ? last_request_target->GetWeakPtr() : nullptr,
          last_target_location, ui::WebInputEventTraits::Clone(event), latency,
          expected_frame_sink_id),
      async_hit_test_timeout_delay_));

  TRACE_EVENT_WITH_FLOW2(
      "viz,benchmark", "Event.Pipeline", TRACE_ID_GLOBAL(trace_id_),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "step",
      "QueryClient", "event", blink::WebInputEvent::GetName(event.GetType()));

  target_client->FrameSinkIdAt(
      target_location, trace_id_,
      base::BindOnce(
          &RenderWidgetTargeter::FoundFrameSinkId,
          weak_ptr_factory_.GetWeakPtr(), root_view->GetWeakPtr(),
          target->GetWeakPtr(), ui::WebInputEventTraits::Clone(event), latency,
          is_verifying ? ++last_verify_request_id_ : ++last_request_id_,
          target_location, std::move(tracker), expected_frame_sink_id));
}

void RenderWidgetTargeter::QueryClient(
    RenderWidgetHostViewBase* root_view,
    RenderWidgetHostViewBase* target,
    const blink::WebInputEvent& event,
    const ui::LatencyInfo& latency,
    const gfx::PointF& target_location,
    RenderWidgetHostViewBase* last_request_target,
    const gfx::PointF& last_target_location) {
  QueryClientInternal(root_view, target, event, latency, target_location,
                      last_request_target, last_target_location,
                      viz::FrameSinkId());
}

void RenderWidgetTargeter::QueryAndVerifyClient(
    RenderWidgetHostViewBase* root_view,
    RenderWidgetHostViewBase* target,
    const blink::WebInputEvent& event,
    const ui::LatencyInfo& latency,
    const gfx::PointF& target_location,
    RenderWidgetHostViewBase* last_request_target,
    const gfx::PointF& last_target_location,
    const viz::FrameSinkId& expected_frame_sink_id) {
  if (verify_request_in_flight_) {
    TargetingRequest request;
    request.root_view = root_view->GetWeakPtr();
    request.event = ui::WebInputEventTraits::Clone(event);
    request.latency = latency;
    request.expected_frame_sink_id = expected_frame_sink_id;
    verify_requests_.push(std::move(request));
    return;
  }
  QueryClientInternal(root_view, target, event, latency, target_location,
                      last_request_target, last_target_location,
                      expected_frame_sink_id);
}

void RenderWidgetTargeter::FlushEventQueue(bool is_verifying) {
  bool events_being_flushed = false;
  bool& request_in_flight =
      is_verifying ? verify_request_in_flight_ : request_in_flight_;
  auto* requests = is_verifying ? &verify_requests_ : &requests_;
  while (!request_in_flight && !requests->empty()) {
    auto request = std::move(requests->front());
    requests->pop();
    // The root-view has gone away. Ignore this event, and try to process the
    // next event.
    if (!request.root_view) {
      continue;
    }
    if (request.tracker)
      request.tracker->Stop();
    // Only notify the delegate once that the current event queue is being
    // flushed. Once all the events are flushed, notify the delegate again.
    if (!is_verifying && !events_being_flushed) {
      delegate_->SetEventsBeingFlushed(true);
      events_being_flushed = true;
    }
    if (is_verifying) {
      QueryAndVerifyClient(request.root_view.get(), request.root_view.get(),
                           *request.event, request.latency,
                           ComputeEventLocation(*request.event), nullptr,
                           gfx::PointF(), request.expected_frame_sink_id);
    } else {
      FindTargetAndDispatch(request.root_view.get(), *request.event,
                            request.latency);
    }
  }
  if (!is_verifying)
    delegate_->SetEventsBeingFlushed(false);
}

void RenderWidgetTargeter::FoundFrameSinkId(
    base::WeakPtr<RenderWidgetHostViewBase> root_view,
    base::WeakPtr<RenderWidgetHostViewBase> target,
    ui::WebScopedInputEvent event,
    const ui::LatencyInfo& latency,
    uint32_t request_id,
    const gfx::PointF& target_location,
    TracingUmaTracker tracker,
    const viz::FrameSinkId& expected_frame_sink_id,
    const viz::FrameSinkId& frame_sink_id,
    const gfx::PointF& transformed_location) {
  if (expected_frame_sink_id.is_valid()) {
    tracker.Stop();
  } else {
    tracker.StopAndRecord();
  }
  uint32_t last_id = expected_frame_sink_id.is_valid() ? last_verify_request_id_
                                                       : last_request_id_;
  bool in_flight = expected_frame_sink_id.is_valid() ? verify_request_in_flight_
                                                     : request_in_flight_;
  if (request_id != last_id || !in_flight) {
    // This is a response to a request that already timed out, so the event
    // should have already been dispatched. Mark the renderer as responsive
    // and otherwise ignore this response.
    unresponsive_views_.erase(target.get());
    return;
  }

  if (expected_frame_sink_id.is_valid()) {
    verify_request_in_flight_ = false;
    async_verify_hit_test_timeout_.reset(nullptr);
  } else {
    request_in_flight_ = false;
    async_hit_test_timeout_.reset(nullptr);

    if (is_viz_hit_testing_debug_enabled_ &&
        event->GetType() == blink::WebInputEvent::Type::kMouseDown) {
      hit_test_async_queried_debug_queue_.push_back(target->GetFrameSinkId());
    }
  }
  auto* view = delegate_->FindViewFromFrameSinkId(frame_sink_id);
  if (!view)
    view = target.get();

  // If a client returned an embedded target, then it might be necessary to
  // continue asking the clients until a client claims an event for itself.
  if (view == target.get() ||
      unresponsive_views_.find(view) != unresponsive_views_.end() ||
      !delegate_->ShouldContinueHitTesting(view)) {
    // Reduced scope is required since FoundTarget can trigger another query
    // which would end up linked to the current query.
    {
      TRACE_EVENT_WITH_FLOW1("viz,benchmark", "Event.Pipeline",
                             TRACE_ID_GLOBAL(trace_id_),
                             TRACE_EVENT_FLAG_FLOW_IN, "step", "FoundTarget");
    }

    FoundTarget(root_view.get(), view, *event, latency, transformed_location,
                false, expected_frame_sink_id);
  } else {
    QueryClientInternal(root_view.get(), view, *event, latency,
                        transformed_location, target.get(), target_location,
                        expected_frame_sink_id);
  }
}

void RenderWidgetTargeter::FoundTarget(
    RenderWidgetHostViewBase* root_view,
    RenderWidgetHostViewBase* target,
    const blink::WebInputEvent& event,
    const ui::LatencyInfo& latency,
    const base::Optional<gfx::PointF>& target_location,
    bool latched_target,
    const viz::FrameSinkId& expected_frame_sink_id) {
  if (SiteIsolationPolicy::UseDedicatedProcessesForAllSites() &&
      !latched_target && !expected_frame_sink_id.is_valid()) {
    UMA_HISTOGRAM_COUNTS_100("Event.AsyncTargeting.AsyncClientDepth",
                             async_depth_);
  }

  // RenderWidgetHostViewMac can be deleted asynchronously, in which case the
  // View will be valid but there will no longer be a RenderWidgetHostImpl.
  if (!root_view || !root_view->GetRenderWidgetHost())
    return;

  if (is_viz_hit_testing_debug_enabled_ &&
      !hit_test_async_queried_debug_queue_.empty()) {
    GetHostFrameSinkManager()->SetHitTestAsyncQueriedDebugRegions(
        root_view->GetRootFrameSinkId(), hit_test_async_queried_debug_queue_);
    hit_test_async_queried_debug_queue_.clear();
  }

  if (features::IsVizHitTestingSurfaceLayerEnabled() &&
      expected_frame_sink_id.is_valid()) {
    static const char* kResultsMatchHistogramName =
        "Event.VizHitTestSurfaceLayer.ResultsMatch";
    bool results_match = target->GetFrameSinkId() == expected_frame_sink_id;
    HitTestResultsMatch match_result =
        HitTestResultsMatch::kHitTestResultChanged;
    if (results_match) {
      match_result = HitTestResultsMatch::kMatch;
    } else {
      // If the results do not match, it is possible that the hit test data
      // changed during verification. We do synchronous hit test again to make
      // sure the result is reliable.
      RenderWidgetTargetResult result =
          delegate_->FindTargetSynchronously(root_view, event);
      if (!result.should_query_view && result.view &&
          expected_frame_sink_id == result.view->GetFrameSinkId()) {
        // If the result did not change, it is likely that viz hit test finds
        // the wrong target.
        match_result = HitTestResultsMatch::kDoNotMatch;
      } else {
        // Hit test data changed, so the result is no longer reliable.
        match_result = HitTestResultsMatch::kHitTestResultChanged;
      }
    }
    UMA_HISTOGRAM_ENUMERATION(kResultsMatchHistogramName, match_result,
                              HitTestResultsMatch::kMaxValue);
    FlushEventQueue(true);
    return;
  }
  delegate_->DispatchEventToTarget(root_view, target, event, latency,
                                   target_location);
  FlushEventQueue(false);
}

void RenderWidgetTargeter::AsyncHitTestTimedOut(
    base::WeakPtr<RenderWidgetHostViewBase> current_request_root_view,
    base::WeakPtr<RenderWidgetHostViewBase> current_request_target,
    const gfx::PointF& current_target_location,
    base::WeakPtr<RenderWidgetHostViewBase> last_request_target,
    const gfx::PointF& last_target_location,
    ui::WebScopedInputEvent event,
    const ui::LatencyInfo& latency,
    const viz::FrameSinkId& expected_frame_sink_id) {
  DCHECK(request_in_flight_ || verify_request_in_flight_);
  // If we time out during a verification, we early out to avoid dispatching
  // event to root frame.
  if (expected_frame_sink_id.is_valid()) {
    verify_request_in_flight_ = false;
    return;
  } else {
    request_in_flight_ = false;
  }

  if (!current_request_root_view)
    return;

  // Mark view as unresponsive so further events will not be sent to it.
  if (current_request_target)
    unresponsive_views_.insert(current_request_target.get());

  if (current_request_root_view.get() == current_request_target.get()) {
    // When a request to the top-level frame times out then the event gets
    // sent there anyway. It will trigger the hung renderer dialog if the
    // renderer fails to process it.
    FoundTarget(current_request_root_view.get(),
                current_request_root_view.get(), *event, latency,
                current_target_location, false, viz::FrameSinkId());
  } else {
    FoundTarget(current_request_root_view.get(), last_request_target.get(),
                *event, latency, last_target_location, false,
                viz::FrameSinkId());
  }
}

}  // namespace content
