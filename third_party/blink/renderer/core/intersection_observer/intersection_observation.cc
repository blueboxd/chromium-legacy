// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/intersection_observer/intersection_observation.h"

#include "third_party/blink/renderer/core/dom/element_rare_data.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_geometry.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_controller.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"

namespace blink {

namespace {


}  // namespace

IntersectionObservation::IntersectionObservation(IntersectionObserver& observer,
                                                 Element& target)
    : observer_(observer),
      target_(&target),
      last_run_time_(-observer.GetEffectiveDelay()),
      last_is_visible_(false),
      // Note that the spec says the initial value of last_threshold_index_
      // should be -1, but since last_threshold_index_ is unsigned, we use a
      // different sentinel value.
      last_threshold_index_(kMaxThresholdIndex - 1) {}

void IntersectionObservation::Compute(unsigned flags) {
  DCHECK(Observer());
  if (!target_ || !observer_->RootIsValid() | !observer_->GetExecutionContext())
    return;
  if (flags &
      (observer_->RootIsImplicit() ? kImplicitRootObserversNeedUpdate
                                   : kExplicitRootObserversNeedUpdate)) {
    needs_update_ = true;
  }
  if (!needs_update_)
    return;
  DOMHighResTimeStamp timestamp = observer_->GetTimeStamp();
  if (timestamp == -1)
    return;
  if (!(flags & kIgnoreDelay) &&
      timestamp - last_run_time_ < observer_->GetEffectiveDelay()) {
    return;
  }
  if (target_->isConnected() && Observer()->trackVisibility()) {
    FrameOcclusionState occlusion_state =
        target_->GetDocument().GetFrame()->GetOcclusionState();
    // If we're tracking visibility, and we don't have occlusion information
    // from our parent frame, then postpone computing intersections until a
    // later lifecycle when the occlusion information is known.
    if (occlusion_state == FrameOcclusionState::kUnknown)
      return;
  }
  last_run_time_ = timestamp;
  needs_update_ = 0;
  Vector<Length> root_margin(4);
  root_margin[0] = observer_->TopMargin();
  root_margin[1] = observer_->RightMargin();
  root_margin[2] = observer_->BottomMargin();
  root_margin[3] = observer_->LeftMargin();
  bool report_root_bounds = observer_->AlwaysReportRootBounds() ||
                            (flags & kReportImplicitRootBounds) ||
                            !observer_->RootIsImplicit();
  unsigned geometry_flags = IntersectionGeometry::kShouldConvertToCSSPixels;
  if (report_root_bounds)
    geometry_flags |= IntersectionGeometry::kShouldReportRootBounds;
  if (Observer()->trackVisibility())
    geometry_flags |= IntersectionGeometry::kShouldComputeVisibility;
  if (Observer()->trackFractionOfRoot())
    geometry_flags |= IntersectionGeometry::kShouldTrackFractionOfRoot;

  IntersectionGeometry geometry(observer_->root(), *Target(), root_margin,
                                observer_->thresholds(), geometry_flags);

  // TODO(tkent): We can't use CHECK_LT due to a compile error.
  CHECK(geometry.ThresholdIndex() < kMaxThresholdIndex - 1);

  if (last_threshold_index_ != geometry.ThresholdIndex() ||
      last_is_visible_ != geometry.IsVisible()) {
    entries_.push_back(MakeGarbageCollected<IntersectionObserverEntry>(
        geometry, timestamp, Target()));
    Observer()->SetNeedsDelivery();
    SetLastThresholdIndex(geometry.ThresholdIndex());
    SetWasVisible(geometry.IsVisible());
  }
}

void IntersectionObservation::TakeRecords(
    HeapVector<Member<IntersectionObserverEntry>>& entries) {
  entries.AppendVector(entries_);
  entries_.clear();
}

void IntersectionObservation::Disconnect() {
  DCHECK(Observer());
  if (target_) {
    Target()->EnsureIntersectionObserverData().RemoveObservation(*Observer());
    if (target_->isConnected() &&
        !Target()->EnsureIntersectionObserverData().HasObservations()) {
      target_->GetDocument()
          .EnsureIntersectionObserverController()
          .RemoveTrackedTarget(*target_);
    }
  }
  entries_.clear();
  observer_.Clear();
}

void IntersectionObservation::Trace(blink::Visitor* visitor) {
  visitor->Trace(observer_);
  visitor->Trace(entries_);
  visitor->Trace(target_);
}

}  // namespace blink
