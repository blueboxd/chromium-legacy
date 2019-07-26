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
    StopButNotRecord();
    UmaHistogramTimes(metric_name_, base::TimeTicks::Now() - start_time_);
  }

  void StopButNotRecord() {
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

RenderWidgetTargeter::TargetingRequest::TargetingRequest(
    base::WeakPtr<RenderWidgetHostViewBase> root_view,
    const blink::WebInputEvent& event,
    const ui::LatencyInfo& latency) {
  this->root_view = std::move(root_view);
  this->location = ComputeEventLocation(event);
  this->event = ui::WebInputEventTraits::Clone(event);
  this->latency = latency;
}

RenderWidgetTargeter::TargetingRequest::TargetingRequest(
    base::WeakPtr<RenderWidgetHostViewBase> root_view,
    const gfx::PointF& location,
    RenderWidgetHostAtPointCallback callback) {
  this->root_view = std::move(root_view);
  this->location = location;
  this->callback = std::move(callback);
}

RenderWidgetTargeter::TargetingRequest::TargetingRequest(
    TargetingRequest&& request) = default;

RenderWidgetTargeter::TargetingRequest& RenderWidgetTargeter::TargetingRequest::
operator=(TargetingRequest&&) = default;

RenderWidgetTargeter::TargetingRequest::~TargetingRequest() = default;

void RenderWidgetTargeter::TargetingRequest::RunCallback(
    RenderWidgetHostViewBase* target,
    base::Optional<gfx::PointF> point) {
  if (!callback.is_null()) {
    std::move(callback).Run(target ? target->GetWeakPtr() : nullptr, point);
  }
}

bool RenderWidgetTargeter::TargetingRequest::MergeEventIfPossible(
    const blink::WebInputEvent& new_event) {
  if (event && !blink::WebInputEvent::IsTouchEventType(new_event.GetType()) &&
      !blink::WebInputEvent::IsGestureEventType(new_event.GetType()) &&
      ui::CanCoalesce(new_event, *event.get())) {
    ui::Coalesce(new_event, event.get());
    return true;
  }
  return false;
}

void RenderWidgetTargeter::TargetingRequest::StartQueueingTimeTracker() {
  tracker =
      std::make_unique<TracingUmaTracker>("Event.AsyncTargeting.TimeInQueue");
}

void RenderWidgetTargeter::TargetingRequest::StopQueueingTimeTracker() {
  if (tracker)
    tracker->StopAndRecord();
}

bool RenderWidgetTargeter::TargetingRequest::IsWebInputEventRequest() const {
  return !!event;
}

const blink::WebInputEvent& RenderWidgetTargeter::TargetingRequest::GetEvent()
    const {
  return *event.get();
}

RenderWidgetHostViewBase* RenderWidgetTargeter::TargetingRequest::GetRootView()
    const {
  return root_view.get();
}

gfx::PointF RenderWidgetTargeter::TargetingRequest::GetLocation() const {
  return location;
}

viz::FrameSinkId
RenderWidgetTargeter::TargetingRequest::GetExpectedFrameSinkId() const {
  return expected_frame_sink_id;
}

void RenderWidgetTargeter::TargetingRequest::SetExpectedFrameSinkId(
    const viz::FrameSinkId& id) {
  expected_frame_sink_id = id;
}

const ui::LatencyInfo& RenderWidgetTargeter::TargetingRequest::GetLatency()
    const {
  return latency;
}

RenderWidgetTargeter::RenderWidgetTargeter(Delegate* delegate)
    : trace_id_(base::RandUint64()),
      is_viz_hit_testing_debug_enabled_(
          features::IsVizHitTestingDebugEnabled()),
      delegate_(delegate) {
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

  if (!requests_.empty()) {
    auto& request = requests_.back();
    if (request.MergeEventIfPossible(event))
      return;
  }

  TargetingRequest request(root_view->GetWeakPtr(), event, latency);

  ResolveTargetingRequest(std::move(request));
}

void RenderWidgetTargeter::FindTargetAndCallback(
    RenderWidgetHostViewBase* root_view,
    const gfx::PointF& point,
    RenderWidgetHostAtPointCallback callback) {
  TargetingRequest request(root_view->GetWeakPtr(), point, std::move(callback));

  ResolveTargetingRequest(std::move(request));
}

void RenderWidgetTargeter::ResolveTargetingRequest(TargetingRequest request) {
  if (request_in_flight_) {
    request.StartQueueingTimeTracker();
    requests_.push(std::move(request));
    return;
  }

  RenderWidgetTargetResult result;
  if (request.IsWebInputEventRequest()) {
    result = delegate_->FindTargetSynchronously(request.GetRootView(),
                                                request.GetEvent());
  } else {
    result = delegate_->FindTargetSynchronouslyAtPoint(request.GetRootView(),
                                                       request.GetLocation());
  }
  RenderWidgetHostViewBase* target = result.view;
  async_depth_ = 0;
  if (result.should_query_view) {
    TRACE_EVENT_WITH_FLOW2(
        "viz,benchmark", "Event.Pipeline", TRACE_ID_GLOBAL(trace_id_),
        TRACE_EVENT_FLAG_FLOW_OUT, "step", "QueryClient(Start)",
        "event_location", request.GetLocation().ToString());

    // TODO(kenrb, sadrul): When all event types support asynchronous hit
    // testing, we should be able to have FindTargetSynchronously return the
    // view and location to use for the renderer hit test query.
    // Currently it has to return the surface hit test target, for event types
    // that ignore |result.should_query_view|, and therefore we have to use
    // root_view and the original event location for the initial query.
    // Do not compare hit test results if we are forced to do async hit testing
    // by HitTestQuery.
    QueryClient(std::move(request));
  } else {
    FoundTarget(target, result.target_location, result.latched_target,
                &request);
    // Verify the event targeting results from surface layer viz hit testing if
    // --use-viz-hit-test-surface-layer is enabled.
    if (result.should_verify_result && !target->IsRenderWidgetHostViewGuest()) {
      request.SetExpectedFrameSinkId(target->GetFrameSinkId());
      QueryAndVerifyClient(std::move(request));
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
    RenderWidgetHostViewBase* target,
    const gfx::PointF& target_location,
    RenderWidgetHostViewBase* last_request_target,
    const gfx::PointF& last_target_location,
    TargetingRequest request) {
  // Async event targeting and verifying use two different queues, so they don't
  // block each other.
  bool is_verifying = request.GetExpectedFrameSinkId().is_valid();
  DCHECK((!is_verifying && !request_in_flight_) ||
         (is_verifying && !verify_request_in_flight_));

  auto* target_client = target->host()->input_target_client();
  // |target_client| may not be set yet for this |target| on Mac, need to
  // understand why this happens. https://crbug.com/859492.
  // We do not verify hit testing result under this circumstance.
  if (!target_client) {
    FoundTarget(target, target_location, false, &request);
    return;
  }

  if (is_verifying) {
    verify_request_in_flight_ = std::move(request);
  } else {
    request_in_flight_ = std::move(request);
    async_depth_++;
  }
  TracingUmaTracker tracker("Event.AsyncTargeting.ResponseTime");
  auto& hit_test_timeout =
      is_verifying ? async_verify_hit_test_timeout_ : async_hit_test_timeout_;
  hit_test_timeout.reset(new OneShotTimeoutMonitor(
      base::BindOnce(
          &RenderWidgetTargeter::AsyncHitTestTimedOut,
          weak_ptr_factory_.GetWeakPtr(), target->GetWeakPtr(), target_location,
          last_request_target ? last_request_target->GetWeakPtr() : nullptr,
          last_target_location, is_verifying),
      async_hit_test_timeout_delay_));

  TRACE_EVENT_WITH_FLOW2(
      "viz,benchmark", "Event.Pipeline", TRACE_ID_GLOBAL(trace_id_),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "step",
      "QueryClient", "event_location", request.GetLocation().ToString());

  target_client->FrameSinkIdAt(
      target_location, trace_id_,
      base::BindOnce(
          &RenderWidgetTargeter::FoundFrameSinkId,
          weak_ptr_factory_.GetWeakPtr(), target->GetWeakPtr(),
          is_verifying ? ++last_verify_request_id_ : ++last_request_id_,
          target_location, std::move(tracker), is_verifying));
}

void RenderWidgetTargeter::QueryClient(TargetingRequest request) {
  auto* target = request.GetRootView();
  auto target_location = request.GetLocation();
  QueryClientInternal(target, target_location, nullptr, gfx::PointF(),
                      std::move(request));
}

void RenderWidgetTargeter::QueryAndVerifyClient(TargetingRequest request) {
  if (verify_request_in_flight_) {
    verify_requests_.push(std::move(request));
    return;
  }
  auto* target = request.GetRootView();
  auto target_location = request.GetLocation();
  QueryClientInternal(target, target_location, nullptr, gfx::PointF(),
                      std::move(request));
}

void RenderWidgetTargeter::FlushEventQueue(bool is_verifying) {
  bool events_being_flushed = false;
  base::Optional<TargetingRequest>& request_in_flight =
      is_verifying ? verify_request_in_flight_ : request_in_flight_;
  auto* requests = is_verifying ? &verify_requests_ : &requests_;
  while (!request_in_flight && !requests->empty()) {
    auto request = std::move(requests->front());
    requests->pop();
    // The root-view has gone away. Ignore this event, and try to process the
    // next event.
    if (!request.GetRootView()) {
      continue;
    }
    request.StopQueueingTimeTracker();
    // Only notify the delegate once that the current event queue is being
    // flushed. Once all the events are flushed, notify the delegate again.
    if (!is_verifying && !events_being_flushed) {
      delegate_->SetEventsBeingFlushed(true);
      events_being_flushed = true;
    }
    if (is_verifying) {
      QueryAndVerifyClient(std::move(request));
    } else {
      ResolveTargetingRequest(std::move(request));
    }
  }
  if (!is_verifying)
    delegate_->SetEventsBeingFlushed(false);
}

void RenderWidgetTargeter::FoundFrameSinkId(
    base::WeakPtr<RenderWidgetHostViewBase> target,
    uint32_t request_id,
    const gfx::PointF& target_location,
    TracingUmaTracker tracker,
    const bool is_verification_request,
    const viz::FrameSinkId& frame_sink_id,
    const gfx::PointF& transformed_location) {
  if (is_verification_request) {
    tracker.StopButNotRecord();
  } else {
    tracker.StopAndRecord();
  }

  uint32_t last_id =
      is_verification_request ? last_verify_request_id_ : last_request_id_;
  bool in_flight = is_verification_request
                       ? verify_request_in_flight_.has_value()
                       : request_in_flight_.has_value();
  if (request_id != last_id || !in_flight) {
    // This is a response to a request that already timed out, so the event
    // should have already been dispatched. Mark the renderer as responsive
    // and otherwise ignore this response.
    unresponsive_views_.erase(target.get());
    return;
  }

  TargetingRequest request = is_verification_request
                                 ? std::move(verify_request_in_flight_.value())
                                 : std::move(request_in_flight_.value());

  if (request.GetExpectedFrameSinkId().is_valid()) {
    verify_request_in_flight_.reset();
    async_verify_hit_test_timeout_.reset(nullptr);
  } else {
    request_in_flight_.reset();
    async_hit_test_timeout_.reset(nullptr);

    if (is_viz_hit_testing_debug_enabled_ && request.IsWebInputEventRequest() &&
        request.GetEvent().GetType() ==
            blink::WebInputEvent::Type::kMouseDown) {
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

    FoundTarget(view, transformed_location, false, &request);
  } else {
    QueryClientInternal(view, transformed_location, target.get(),
                        target_location, std::move(request));
  }
}

void RenderWidgetTargeter::FoundTarget(
    RenderWidgetHostViewBase* target,
    const base::Optional<gfx::PointF>& target_location,
    bool latched_target,
    TargetingRequest* request) {
  DCHECK(request);

  if (SiteIsolationPolicy::UseDedicatedProcessesForAllSites() &&
      !latched_target && !request->GetExpectedFrameSinkId().is_valid()) {
    UMA_HISTOGRAM_COUNTS_100("Event.AsyncTargeting.AsyncClientDepth",
                             async_depth_);
  }

  // RenderWidgetHostViewMac can be deleted asynchronously, in which case the
  // View will be valid but there will no longer be a RenderWidgetHostImpl.
  if (!request->GetRootView() || !request->GetRootView()->GetRenderWidgetHost())
    return;

  if (is_viz_hit_testing_debug_enabled_ &&
      !hit_test_async_queried_debug_queue_.empty()) {
    GetHostFrameSinkManager()->SetHitTestAsyncQueriedDebugRegions(
        request->GetRootView()->GetRootFrameSinkId(),
        hit_test_async_queried_debug_queue_);
    hit_test_async_queried_debug_queue_.clear();
  }

  if (features::IsVizHitTestingSurfaceLayerEnabled() &&
      request->GetExpectedFrameSinkId().is_valid()) {
    static const char* kResultsMatchHistogramName =
        "Event.VizHitTestSurfaceLayer.ResultsMatch";
    bool results_match =
        target->GetFrameSinkId() == request->GetExpectedFrameSinkId();
    HitTestResultsMatch match_result =
        HitTestResultsMatch::kHitTestResultChanged;
    if (results_match) {
      match_result = HitTestResultsMatch::kMatch;
    } else {
      // If the results do not match, it is possible that the hit test data
      // changed during verification. We do synchronous hit test again to make
      // sure the result is reliable.
      RenderWidgetTargetResult result;
      if (request->IsWebInputEventRequest()) {
        result = delegate_->FindTargetSynchronously(request->GetRootView(),
                                                    request->GetEvent());
      } else {
        result = delegate_->FindTargetSynchronouslyAtPoint(
            request->GetRootView(), request->GetLocation());
      }

      if (!result.should_query_view && result.view &&
          request->GetExpectedFrameSinkId() == result.view->GetFrameSinkId()) {
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

  if (request->IsWebInputEventRequest()) {
    delegate_->DispatchEventToTarget(request->GetRootView(), target,
                                     request->GetEvent(), request->GetLatency(),
                                     target_location);
  } else {
    request->RunCallback(target, target_location);
  }

  FlushEventQueue(false);
}

void RenderWidgetTargeter::AsyncHitTestTimedOut(
    base::WeakPtr<RenderWidgetHostViewBase> current_request_target,
    const gfx::PointF& current_target_location,
    base::WeakPtr<RenderWidgetHostViewBase> last_request_target,
    const gfx::PointF& last_target_location,
    const bool is_verification_request) {
  DCHECK(request_in_flight_ || verify_request_in_flight_);

  TargetingRequest request = is_verification_request
                                 ? std::move(verify_request_in_flight_.value())
                                 : std::move(request_in_flight_.value());

  // If we time out during a verification, we early out to avoid dispatching
  // event to root frame.
  if (request.GetExpectedFrameSinkId().is_valid()) {
    verify_request_in_flight_.reset();
    return;
  } else {
    request_in_flight_.reset();
  }

  if (!request.GetRootView())
    return;

  // Mark view as unresponsive so further events will not be sent to it.
  if (current_request_target)
    unresponsive_views_.insert(current_request_target.get());

  if (request.GetRootView() == current_request_target.get()) {
    // When a request to the top-level frame times out then the event gets
    // sent there anyway. It will trigger the hung renderer dialog if the
    // renderer fails to process it.
    FoundTarget(current_request_target.get(), current_target_location, false,
                &request);
  } else {
    FoundTarget(last_request_target.get(), last_target_location, false,
                &request);
  }
}

}  // namespace content
