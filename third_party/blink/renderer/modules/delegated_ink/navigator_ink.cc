// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/delegated_ink/navigator_ink.h"

#include "third_party/blink/renderer/modules/delegated_ink/ink.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

const char NavigatorInk::kSupplementName[] = "NavigatorInk";

NavigatorInk::NavigatorInk(Navigator& navigator)
    : Supplement<Navigator>(navigator), ink_(MakeGarbageCollected<Ink>()) {}

Ink* NavigatorInk::ink(Navigator& navigator) {
  DCHECK(RuntimeEnabledFeatures::DelegatedInkTrailsEnabled());

  NavigatorInk* supplement =
      Supplement<Navigator>::From<NavigatorInk>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorInk>(navigator);
    ProvideTo(navigator, supplement);
  }

  return supplement->ink_;
}

void NavigatorInk::Trace(Visitor* visitor) {
  visitor->Trace(ink_);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink
