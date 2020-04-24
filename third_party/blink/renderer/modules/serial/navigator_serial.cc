// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/serial/navigator_serial.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/serial/serial.h"

namespace blink {

const char NavigatorSerial::kSupplementName[] = "NavigatorSerial";

NavigatorSerial& NavigatorSerial::From(Navigator& navigator) {
  NavigatorSerial* supplement =
      Supplement<Navigator>::From<NavigatorSerial>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorSerial>(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

Serial* NavigatorSerial::serial(Navigator& navigator) {
  return NavigatorSerial::From(navigator).serial();
}

void NavigatorSerial::Trace(Visitor* visitor) {
  visitor->Trace(serial_);
  Supplement<Navigator>::Trace(visitor);
}

NavigatorSerial::NavigatorSerial(Navigator& navigator)
    : Supplement<Navigator>(navigator) {
  if (navigator.DomWindow()) {
    serial_ = MakeGarbageCollected<Serial>(*navigator.DomWindow());
  }
}

}  // namespace blink
