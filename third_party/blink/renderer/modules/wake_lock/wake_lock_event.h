// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_WAKE_LOCK_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_WAKE_LOCK_EVENT_H_

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class WakeLockEventInit;
class WakeLockSentinel;

class WakeLockEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static WakeLockEvent* Create(const AtomicString& type,
                               const WakeLockEventInit* initializer);

  WakeLockEvent(const AtomicString& type, const WakeLockEventInit* initializer);
  WakeLockEvent(const AtomicString& type, WakeLockSentinel* lock);
  ~WakeLockEvent() override;

  // Web-exposed APIs.
  WakeLockSentinel* lock() const;

  // Event overrides.
  const AtomicString& InterfaceName() const override;
  void Trace(blink::Visitor* visitor) override;

 private:
  const Member<WakeLockSentinel> lock_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_WAKE_LOCK_EVENT_H_
