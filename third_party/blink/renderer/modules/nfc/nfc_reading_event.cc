// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/nfc_reading_event.h"

#include "third_party/blink/renderer/modules/nfc/ndef_message.h"
#include "third_party/blink/renderer/modules/nfc/nfc_reading_event_init.h"

namespace blink {

NFCReadingEvent::NFCReadingEvent(const AtomicString& event_type,
                                 const NFCReadingEventInit* initializer)
    : Event(event_type, initializer),
      serial_number_(initializer->serialNumber()),
      message_(NDEFMessage::Create(initializer->message())) {}

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
