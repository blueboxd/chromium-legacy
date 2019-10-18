// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/wake_lock/wake_lock_sentinel.h"

#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/event_target_modules_names.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock_manager.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

WakeLockSentinel::WakeLockSentinel(ScriptState* script_state,
                                   WakeLockType type,
                                   WakeLockManager* manager)
    : ContextLifecycleObserver(ExecutionContext::From(script_state)),
      manager_(manager),
      type_(type) {}

WakeLockSentinel::~WakeLockSentinel() = default;

ScriptPromise WakeLockSentinel::release(ScriptState* script_state) {
  DoRelease();
  return ScriptPromise::CastUndefined(script_state);
}

String WakeLockSentinel::type() const {
  switch (type_) {
    case WakeLockType::kScreen:
      return "screen";
    case WakeLockType::kSystem:
      return "system";
  }
}

ExecutionContext* WakeLockSentinel::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

const AtomicString& WakeLockSentinel::InterfaceName() const {
  return event_target_names::kWakeLockSentinel;
}

void WakeLockSentinel::Trace(blink::Visitor* visitor) {
  visitor->Trace(manager_);
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

bool WakeLockSentinel::HasPendingActivity() const {
  return HasEventListeners();
}

void WakeLockSentinel::ContextDestroyed(ExecutionContext*) {
  // Release all event listeners so that HasPendingActivity() does not return
  // true forever once a listener has been added to the object.
  RemoveAllEventListeners();
  DCHECK(!HasEventListeners());
}

void WakeLockSentinel::DoRelease() {
  if (!manager_)
    return;

  manager_->UnregisterSentinel(this);
  manager_.Clear();

  DispatchEvent(*Event::Create(event_type_names::kRelease));
}

}  // namespace blink
