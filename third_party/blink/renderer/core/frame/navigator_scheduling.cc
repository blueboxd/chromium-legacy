// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/navigator_scheduling.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/scheduling.h"

namespace blink {

const char NavigatorScheduling::kSupplementName[] = "NavigatorScheduling";

NavigatorScheduling& NavigatorScheduling::From(Navigator& navigator) {
  NavigatorScheduling* supplement =
      Supplement<Navigator>::From<NavigatorScheduling>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorScheduling>(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

Scheduling* NavigatorScheduling::scheduling(Navigator& navigator) {
  return From(navigator).scheduling();
}

Scheduling* NavigatorScheduling::scheduling() {
  return scheduling_;
}

void NavigatorScheduling::Trace(Visitor* visitor) {
  visitor->Trace(scheduling_);
  Supplement<Navigator>::Trace(visitor);
}

NavigatorScheduling::NavigatorScheduling(Navigator& navigator)
    : Supplement<Navigator>(navigator) {
  scheduling_ = MakeGarbageCollected<Scheduling>();
}

}  // namespace blink
