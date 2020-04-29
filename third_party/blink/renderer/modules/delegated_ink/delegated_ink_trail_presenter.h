// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_DELEGATED_INK_DELEGATED_INK_TRAIL_PRESENTER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_DELEGATED_INK_DELEGATED_INK_TRAIL_PRESENTER_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class Element;
class InkTrailStyle;
class PointerEvent;

class DelegatedInkTrailPresenter : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit DelegatedInkTrailPresenter(Element* element);
  void updateInkTrailStartPoint(PointerEvent* evt, InkTrailStyle* style);
  uint32_t expectedImprovement() const { return expected_improvement_; }
  Element* presentationArea() const { return presentation_area_; }

  void Trace(Visitor* visitor) override;

 private:
  Member<Element> presentation_area_;
  uint32_t expected_improvement_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_DELEGATED_INK_DELEGATED_INK_TRAIL_PRESENTER_H_
