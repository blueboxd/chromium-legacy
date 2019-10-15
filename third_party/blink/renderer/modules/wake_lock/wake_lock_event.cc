// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/wake_lock/wake_lock_event.h"

#include "base/logging.h"
#include "third_party/blink/renderer/modules/event_interface_modules_names.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock_event_init.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock_sentinel.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

// static
WakeLockEvent* WakeLockEvent::Create(const AtomicString& type,
                                     const WakeLockEventInit* initializer) {
  DCHECK(initializer->hasLock());
  return MakeGarbageCollected<WakeLockEvent>(type, initializer);
}

WakeLockEvent::WakeLockEvent(const AtomicString& type,
                             const WakeLockEventInit* initializer)
    : Event(type, initializer), lock_(initializer->lock()) {
  DCHECK_NE(nullptr, lock_);
}

WakeLockEvent::WakeLockEvent(const AtomicString& type, WakeLockSentinel* lock)
    : Event(type, Bubbles::kNo, Cancelable::kNo), lock_(lock) {}

WakeLockEvent::~WakeLockEvent() = default;

WakeLockSentinel* WakeLockEvent::lock() const {
  return lock_;
}

const AtomicString& WakeLockEvent::InterfaceName() const {
  return event_interface_names::kWakeLockEvent;
}

void WakeLockEvent::Trace(blink::Visitor* visitor) {
  visitor->Trace(lock_);
  Event::Trace(visitor);
}

}  // namespace blink
