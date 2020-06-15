// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tracing_lifecycle_unit_observer.h"

#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom.h"

namespace resource_coordinator {

namespace {

bool IsPendingState(LifecycleUnitState state) {
  return state == LifecycleUnitState::PENDING_FREEZE ||
         state == LifecycleUnitState::PENDING_UNFREEZE;
}

// An event name must be a string that is never destroyed.
const char* GetTabStateEventName(LifecycleUnitState state) {
  switch (state) {
    case LifecycleUnitState::PENDING_FREEZE:
      return kPendingFreezeTracingEventName;
    case LifecycleUnitState::PENDING_UNFREEZE:
      return kPendingUnfreezeTracingEventName;
    default:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace

const char kPendingFreezeTracingEventName[] = "Tab State: Pending Freeze";
const char kPendingUnfreezeTracingEventName[] = "Tab State: Pending Unfreeze";

TracingLifecycleUnitObserver::TracingLifecycleUnitObserver() = default;
TracingLifecycleUnitObserver::~TracingLifecycleUnitObserver() = default;

void TracingLifecycleUnitObserver::OnLifecycleUnitStateChanged(
    LifecycleUnit* lifecycle_unit,
    LifecycleUnitState last_state,
    LifecycleUnitStateChangeReason reason) {
  const LifecycleUnitState current_state = lifecycle_unit->GetState();
  if (IsPendingState(current_state)) {
    // Emit a state begin event if the current state is a pending state.
    EmitStateBeginEvent(GetTabStateEventName(current_state),
                        base::UTF16ToUTF8(lifecycle_unit->GetTitle()).c_str());
  } else {
    // Emit a state end event if the previous state is a pending state.
    MaybeEmitStateEndEvent(last_state);
  }
}

void TracingLifecycleUnitObserver::OnLifecycleUnitDestroyed(
    LifecycleUnit* lifecycle_unit) {
  // Emit a state end event if the final state is a pending state.
  MaybeEmitStateEndEvent(lifecycle_unit->GetState());

  // This is a self-owned object that destroys itself with the LifecycleUnit
  // that it observes.
  lifecycle_unit->RemoveObserver(this);
  delete this;
}

void TracingLifecycleUnitObserver::MaybeEmitStateEndEvent(
    LifecycleUnitState state) {
  if (IsPendingState(state))
    EmitStateEndEvent(GetTabStateEventName(state));
}

void TracingLifecycleUnitObserver::EmitStateBeginEvent(const char* event_name,
                                                       const char* title) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("browser", event_name, this, "Title",
                                    title);
}

void TracingLifecycleUnitObserver::EmitStateEndEvent(const char* event_name) {
  TRACE_EVENT_NESTABLE_ASYNC_END0("browser", event_name, this);
}

}  // namespace resource_coordinator
