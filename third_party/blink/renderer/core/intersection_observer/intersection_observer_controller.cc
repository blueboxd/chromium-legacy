// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_controller.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/frame_owner.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observation.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

IntersectionObserverController::IntersectionObserverController(
    Document* document)
    : ContextClient(document) {}

IntersectionObserverController::~IntersectionObserverController() = default;

void IntersectionObserverController::PostTaskToDeliverObservations() {
  DCHECK(GetExecutionContext());
  // TODO(ojan): These tasks decide whether to throttle a subframe, so they
  // need to be unthrottled, but we should throttle all the other tasks
  // (e.g. ones coming from the web page).
  GetExecutionContext()
      ->GetTaskRunner(TaskType::kInternalIntersectionObserver)
      ->PostTask(
          FROM_HERE,
          WTF::Bind(
              &IntersectionObserverController::DeliverIntersectionObservations,
              WrapWeakPersistent(this),
              IntersectionObserver::kPostTaskToDeliver));
}

void IntersectionObserverController::ScheduleIntersectionObserverForDelivery(
    IntersectionObserver& observer) {
  pending_intersection_observers_.insert(&observer);
  if (observer.GetDeliveryBehavior() ==
      IntersectionObserver::kPostTaskToDeliver)
    PostTaskToDeliverObservations();
}

void IntersectionObserverController::DeliverIntersectionObservations(
    IntersectionObserver::DeliveryBehavior behavior) {
  ExecutionContext* context = GetExecutionContext();
  if (!context) {
    pending_intersection_observers_.clear();
    return;
  }
  // TODO(yukishiino): Remove this CHECK once https://crbug.com/809784 gets
  // resolved.
  CHECK(!context->IsContextDestroyed());
  for (auto& observer : pending_intersection_observers_) {
    if (observer->GetDeliveryBehavior() == behavior)
      intersection_observers_being_invoked_.push_back(observer);
  }
  for (auto& observer : intersection_observers_being_invoked_) {
    pending_intersection_observers_.erase(observer);
    observer->Deliver();
  }
  intersection_observers_being_invoked_.clear();
}

bool IntersectionObserverController::ComputeTrackedIntersectionObservations(
    unsigned flags) {
  bool needs_occlusion_tracking = false;
  if (Document* document = To<Document>(GetExecutionContext())) {
    TRACE_EVENT0("blink",
                 "IntersectionObserverController::"
                 "computeTrackedIntersectionObservations");
    HeapVector<Member<Element>> elements_to_process;
    CopyToVector(tracked_observation_targets_, elements_to_process);
    for (auto& element : elements_to_process) {
      needs_occlusion_tracking |=
          element->ComputeIntersectionObservations(flags);
    }
  }
  return needs_occlusion_tracking;
}

void IntersectionObserverController::AddTrackedTarget(Element& target,
                                                      bool track_occlusion) {
  tracked_observation_targets_.insert(&target);
  if (!track_occlusion)
    return;
  if (LocalFrameView* frame_view = target.GetDocument().View()) {
    if (FrameOwner* frame_owner = frame_view->GetFrame().Owner()) {
      // Set this bit as early as possible, rather than waiting for a lifecycle
      // update to recompute it.
      frame_owner->SetNeedsOcclusionTracking(true);
    }
  }
}

void IntersectionObserverController::RemoveTrackedTarget(Element& target) {
  // Note that we don't try to opportunistically turn off the 'needs occlusion
  // tracking' bit here, like the way we turn it on in AddTrackedTarget. The
  // bit will get recomputed on the next lifecycle update; there's no
  // compelling reason to do it here, so we avoid the iteration through targets
  // and observations here.
  tracked_observation_targets_.erase(&target);
}

void IntersectionObserverController::Trace(blink::Visitor* visitor) {
  visitor->Trace(tracked_observation_targets_);
  visitor->Trace(pending_intersection_observers_);
  visitor->Trace(intersection_observers_being_invoked_);
  ContextClient::Trace(visitor);
}

}  // namespace blink
