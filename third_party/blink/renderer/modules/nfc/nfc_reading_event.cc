// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/nfc_reading_event.h"

#include "third_party/blink/renderer/modules/nfc/ndef_message.h"
#include "third_party/blink/renderer/modules/nfc/nfc_reading_event_init.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

// static
NFCReadingEvent* NFCReadingEvent::Create(const AtomicString& event_type,
                                         const NFCReadingEventInit* init,
                                         ExceptionState& exception_state) {
  NDEFMessage* message = NDEFMessage::Create(init->message(), exception_state);
  if (exception_state.HadException())
    return nullptr;
  DCHECK(message);
  return MakeGarbageCollected<NFCReadingEvent>(event_type, init, message);
}

NFCReadingEvent::NFCReadingEvent(const AtomicString& event_type,
                                 const NFCReadingEventInit* init,
                                 NDEFMessage* message)
    : Event(event_type, init),
      serial_number_(init->serialNumber()),
      message_(message) {}

NFCReadingEvent::NFCReadingEvent(const AtomicString& event_type,
                                 const String& serial_number,
                                 NDEFMessage* message)
    : Event(event_type, Bubbles::kNo, Cancelable::kNo),
      serial_number_(serial_number),
      message_(message) {}

NFCReadingEvent::~NFCReadingEvent() = default;

const AtomicString& NFCReadingEvent::InterfaceName() const {
  return event_interface_names::kNFCReadingEvent;
}

void NFCReadingEvent::Trace(blink::Visitor* visitor) {
  visitor->Trace(message_);
  Event::Trace(visitor);
}

const String& NFCReadingEvent::serialNumber() const {
  if (serial_number_.IsNull())
    return g_empty_string;
  return serial_number_;
}

NDEFMessage* NFCReadingEvent::message() const {
  return message_;
}

}  // namespace blink
