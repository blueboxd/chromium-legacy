// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TRACING_LIFECYCLE_UNIT_OBSERVER_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TRACING_LIFECYCLE_UNIT_OBSERVER_H_

#include "base/macros.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_observer.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-forward.h"

namespace resource_coordinator {

using ::mojom::LifecycleUnitState;

extern const char kPendingFreezeTracingEventName[];
extern const char kPendingUnfreezeTracingEventName[];

// Observes a LifecycleUnit to generate trace events.
class TracingLifecycleUnitObserver : public LifecycleUnitObserver {
 public:
  TracingLifecycleUnitObserver();
  ~TracingLifecycleUnitObserver() override;

  // LifecycleUnitObserver:
  void OnLifecycleUnitStateChanged(
      LifecycleUnit* lifecycle_unit,
      LifecycleUnitState last_state,
      LifecycleUnitStateChangeReason reason) override;
  void OnLifecycleUnitDestroyed(LifecycleUnit* lifecycle_unit) override;

 private:
  // Emits a state end event if |state| is a pending state.
  void MaybeEmitStateEndEvent(LifecycleUnitState state);

  // Emits tracing events. Virtual for testing.
  virtual void EmitStateBeginEvent(const char* event_name, const char* title);
  virtual void EmitStateEndEvent(const char* event_name);

  DISALLOW_COPY_AND_ASSIGN(TracingLifecycleUnitObserver);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TRACING_LIFECYCLE_UNIT_OBSERVER_H_
