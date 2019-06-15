// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NFC_ERROR_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NFC_ERROR_EVENT_H_

#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/modules/nfc/nfc_error_event_init.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class NFCErrorEvent : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static NFCErrorEvent* Create(const AtomicString& event_type,
                               const NFCErrorEventInit* initializer) {
    return MakeGarbageCollected<NFCErrorEvent>(event_type, initializer);
  }

  NFCErrorEvent(const AtomicString& event_type, DOMException* error);
  NFCErrorEvent(const AtomicString& event_type,
                const NFCErrorEventInit* initializer);
  ~NFCErrorEvent() override;

  void Trace(blink::Visitor*) override;

  const AtomicString& InterfaceName() const override;

  DOMException* error() { return error_; }

 private:
  Member<DOMException> error_;
};

DEFINE_TYPE_CASTS(NFCErrorEvent,
                  Event,
                  event,
                  event->InterfaceName() ==
                      event_interface_names::kNFCErrorEvent,
                  event.InterfaceName() ==
                      event_interface_names::kNFCErrorEvent);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NFC_ERROR_EVENT_H_
