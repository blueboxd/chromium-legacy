// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_RESIZE_OBSERVER_RESIZE_OBSERVER_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_RESIZE_OBSERVER_RESIZE_OBSERVER_CONTROLLER_H_

#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class ResizeObserver;

// ResizeObserverController keeps track of all ResizeObservers
// in a single Document.
//
// The observation API is used to integrate ResizeObserver
// and the event loop. It delivers notification in a loop.
// In each iteration, only notifications deeper than the
// shallowest notification from previous iteration are delivered.
class ResizeObserverController final
    : public GarbageCollected<ResizeObserverController> {
 public:
  static const size_t kDepthBottom = 4096;

  ResizeObserverController();

  void AddObserver(ResizeObserver&);

  // observation API
  // Returns min depth of shallowest observed node, kDepthLimit if none.
  size_t GatherObservations();
  // Returns true if gatherObservations has skipped observations
  // because they were too shallow.
  bool SkippedObservations();
  void DeliverObservations();
  void ClearObservations();

  void ClearMinDepth() { min_depth_ = 0; }

  void Trace(Visitor*);

  // For testing only.
  const HeapLinkedHashSet<WeakMember<ResizeObserver>>& Observers() {
    return observers_;
  }

 private:
  // Active observers
  HeapLinkedHashSet<WeakMember<ResizeObserver>> observers_;
  // Minimum depth for observations to be active
  size_t min_depth_ = 0;
};

}  // namespace blink

#endif
