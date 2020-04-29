// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/delegated_ink/delegated_ink_trail_presenter.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/modules/delegated_ink/ink_trail_style.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

DelegatedInkTrailPresenter::DelegatedInkTrailPresenter(Element* element)
    : presentation_area_(element) {}

void DelegatedInkTrailPresenter::updateInkTrailStartPoint(
    PointerEvent* evt,
    InkTrailStyle* style) {
  DCHECK(RuntimeEnabledFeatures::DelegatedInkTrailsEnabled());
  return;
}

void DelegatedInkTrailPresenter::Trace(Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(presentation_area_);
}

}  // namespace blink
