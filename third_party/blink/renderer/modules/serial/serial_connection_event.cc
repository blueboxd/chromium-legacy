// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/serial/serial_connection_event.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_serial_connection_event_init.h"
#include "third_party/blink/renderer/modules/serial/serial_port.h"

namespace blink {

SerialConnectionEvent* SerialConnectionEvent::Create(
    const AtomicString& type,
    const SerialConnectionEventInit* initializer) {
  return MakeGarbageCollected<SerialConnectionEvent>(type, initializer);
}

SerialConnectionEvent* SerialConnectionEvent::Create(const AtomicString& type,
                                                     SerialPort* port) {
  return MakeGarbageCollected<SerialConnectionEvent>(type, port);
}

SerialConnectionEvent::SerialConnectionEvent(
    const AtomicString& type,
    const SerialConnectionEventInit* initializer)
    : Event(type, initializer), port_(initializer->port()) {}

SerialConnectionEvent::SerialConnectionEvent(const AtomicString& type,
                                             SerialPort* port)
    : Event(type, Bubbles::kNo, Cancelable::kNo), port_(port) {}

void SerialConnectionEvent::Trace(Visitor* visitor) {
  visitor->Trace(port_);
  Event::Trace(visitor);
}

}  // namespace blink
