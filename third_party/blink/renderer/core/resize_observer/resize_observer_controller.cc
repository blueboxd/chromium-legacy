// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/resize_observer/resize_observer_controller.h"

#include "third_party/blink/renderer/core/resize_observer/resize_observer.h"

namespace blink {

ResizeObserverController::ResizeObserverController() = default;

void ResizeObserverController::AddObserver(ResizeObserver& observer) {
  observers_.insert(&observer);
}

size_t ResizeObserverController::GatherObservations() {
  size_t shallowest = ResizeObserverController::kDepthBottom;

  for (auto& observer : observers_) {
    size_t depth = observer->GatherObservations(min_depth_);
    if (depth < shallowest)
      shallowest = depth;
  }
  min_depth_ = shallowest;
  return min_depth_;
}

bool ResizeObserverController::SkippedObservations() {
  for (auto& observer : observers_) {
    if (observer->SkippedObservations())
      return true;
  }
  return false;
}

void ResizeObserverController::DeliverObservations() {
  // Copy is needed because m_observers might get modified during
  // deliverObservations.
  HeapVector<Member<ResizeObserver>> observers;
  CopyToVector(observers_, observers);

  for (auto& observer : observers) {
    if (observer) {
      observer->DeliverObservations();
    }
  }
}

void ResizeObserverController::ClearObservations() {
  for (auto& observer : observers_)
    observer->ClearObservations();
}

void ResizeObserverController::Trace(Visitor* visitor) {
  visitor->Trace(observers_);
}

}  // namespace blink
