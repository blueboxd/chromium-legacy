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

void IntersectionObserverController::PostTaskToDeliverNotifications() {
  DCHECK(GetExecutionContext());
  GetExecutionContext()
      ->GetTaskRunner(TaskType::kInternalIntersectionObserver)
      ->PostTask(
          FROM_HERE,
          WTF::Bind(&IntersectionObserverController::DeliverNotifications,
                    WrapWeakPersistent(this),
                    IntersectionObserver::kPostTaskToDeliver));
}

void IntersectionObserverController::ScheduleIntersectionObserverForDelivery(
    IntersectionObserver& observer) {
  pending_intersection_observers_.insert(&observer);
  if (observer.GetDeliveryBehavior() ==
      IntersectionObserver::kPostTaskToDeliver)
    PostTaskToDeliverNotifications();
}

void IntersectionObserverController::DeliverNotifications(
    IntersectionObserver::DeliveryBehavior behavior) {
  ExecutionContext* context = GetExecutionContext();
  if (!context) {
    pending_intersection_observers_.clear();
    return;
  }
  HeapVector<Member<IntersectionObserver>> intersection_observers_being_invoked;
  for (auto& observer : pending_intersection_observers_) {
    if (observer->GetDeliveryBehavior() == behavior)
      intersection_observers_being_invoked.push_back(observer);
  }
  for (auto& observer : intersection_observers_being_invoked) {
    pending_intersection_observers_.erase(observer);
    observer->Deliver();
  }
}

bool IntersectionObserverController::ComputeIntersections(unsigned flags) {
  needs_occlusion_tracking_ = false;
  if (Document* document = To<Document>(GetExecutionContext())) {
    TRACE_EVENT0("blink",
                 "IntersectionObserverController::"
                 "computeIntersections");
    HeapVector<Member<IntersectionObserver>> observers_to_process;
    CopyToVector(explicit_root_observers_, observers_to_process);
    for (auto& observer : observers_to_process) {
      needs_occlusion_tracking_ |= observer->ComputeIntersections(flags);
    }
    HeapVector<Member<IntersectionObservation>> observations_to_process;
    CopyToVector(implicit_root_observations_, observations_to_process);
    for (auto& observation : observations_to_process) {
      observation->ComputeIntersection(flags);
      needs_occlusion_tracking_ |= observation->Observer()->trackVisibility();
    }
  }
  return needs_occlusion_tracking_;
}

void IntersectionObserverController::AddTrackedObserver(
    IntersectionObserver& observer,
    bool track_occlusion) {
  DCHECK(!observer.RootIsImplicit());
  explicit_root_observers_.insert(&observer);
  if (!track_occlusion)
    return;
  needs_occlusion_tracking_ = true;
  if (LocalFrameView* frame_view = observer.root()->GetDocument().View()) {
    if (FrameOwner* frame_owner = frame_view->GetFrame().Owner()) {
      // Set this bit as early as possible, rather than waiting for a lifecycle
      // update to recompute it.
      frame_owner->SetNeedsOcclusionTracking(true);
    }
  }
}

void IntersectionObserverController::RemoveTrackedObserver(
    IntersectionObserver& observer) {
  DCHECK(!observer.RootIsImplicit());
  // Note that we don't try to opportunistically turn off the 'needs occlusion
  // tracking' bit here, like the way we turn it on in AddTrackedObserver. The
  // bit will get recomputed on the next lifecycle update; there's no
  // compelling reason to do it here, so we avoid the iteration through
  // observers and observations here.
  explicit_root_observers_.erase(&observer);
}

void IntersectionObserverController::AddTrackedObservation(
    IntersectionObservation& observation,
    bool track_occlusion) {
  DCHECK(observation.Observer()->RootIsImplicit());
  implicit_root_observations_.insert(&observation);
  if (!track_occlusion)
    return;
  needs_occlusion_tracking_ = true;
  if (LocalFrameView* frame_view = observation.Target()->GetDocument().View()) {
    if (FrameOwner* frame_owner = frame_view->GetFrame().Owner()) {
      frame_owner->SetNeedsOcclusionTracking(true);
    }
  }
}

void IntersectionObserverController::RemoveTrackedObservation(
    IntersectionObservation& observation) {
  DCHECK(observation.Observer()->RootIsImplicit());
  implicit_root_observations_.erase(&observation);
}

void IntersectionObserverController::Trace(blink::Visitor* visitor) {
  visitor->Trace(explicit_root_observers_);
  visitor->Trace(implicit_root_observations_);
  visitor->Trace(pending_intersection_observers_);
  ContextClient::Trace(visitor);
}

}  // namespace blink
