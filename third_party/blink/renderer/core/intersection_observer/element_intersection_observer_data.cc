// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/intersection_observer/element_intersection_observer_data.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observation.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_controller.h"

namespace blink {

ElementIntersectionObserverData::ElementIntersectionObserverData() = default;

IntersectionObservation* ElementIntersectionObserverData::GetObservationFor(
    IntersectionObserver& observer) {
  auto i = intersection_observations_.find(&observer);
  if (i == intersection_observations_.end())
    return nullptr;
  return i->value;
}

void ElementIntersectionObserverData::AddObservation(
    IntersectionObservation& observation) {
  DCHECK(observation.Observer());
  intersection_observations_.insert(observation.Observer(), &observation);
}

void ElementIntersectionObserverData::AddObserver(
    IntersectionObserver& observer) {
  intersection_observers_.insert(&observer);
}

bool ElementIntersectionObserverData::IsTargetOfImplicitRootObserver() const {
  for (auto& entry : intersection_observations_) {
    if (entry.key->RootIsImplicit())
      return true;
  }
  return false;
}

void ElementIntersectionObserverData::RemoveObservation(
    IntersectionObserver& observer) {
  intersection_observations_.erase(&observer);
}

bool ElementIntersectionObserverData::ComputeIntersectionsForTarget(
    unsigned flags) {
  bool needs_occlusion_tracking = false;
  for (auto& entry : intersection_observations_) {
    needs_occlusion_tracking |= entry.key->NeedsOcclusionTracking();
    entry.value->ComputeIntersection(flags);
  }
  return needs_occlusion_tracking;
}

bool ElementIntersectionObserverData::ComputeIntersectionsForLifecycleUpdate(
    unsigned flags) {
  bool needs_occlusion_tracking = false;

  // Process explicit-root observers for which this element is root.
  for (auto& observer : intersection_observers_) {
    needs_occlusion_tracking |= observer->NeedsOcclusionTracking();
    if (flags & IntersectionObservation::kExplicitRootObserversNeedUpdate) {
      observer->ComputeIntersections(flags);
    }
  }

  // Process implicit-root observations for which this element is target.
  unsigned implicit_root_flags =
      flags & ~IntersectionObservation::kExplicitRootObserversNeedUpdate;
  needs_occlusion_tracking |=
      ComputeIntersectionsForTarget(implicit_root_flags);
  return needs_occlusion_tracking;
}

bool ElementIntersectionObserverData::NeedsOcclusionTracking() const {
  for (auto& entry : intersection_observations_) {
    if (entry.key->trackVisibility())
      return true;
  }
  return false;
}

void ElementIntersectionObserverData::Trace(blink::Visitor* visitor) {
  visitor->Trace(intersection_observations_);
  visitor->Trace(intersection_observers_);
}

}  // namespace blink
