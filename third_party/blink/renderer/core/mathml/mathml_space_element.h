// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_MATHML_MATHML_SPACE_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_MATHML_MATHML_SPACE_ELEMENT_H_

#include "third_party/blink/renderer/core/mathml/mathml_element.h"

namespace blink {

class LayoutObject;
class ComputedStyle;
class CSSToLengthConversionData;

class MathMLSpaceElement final : public MathMLElement {
 public:
  explicit MathMLSpaceElement(Document&);

  void AddMathBaselineIfNeeded(ComputedStyle&,
                               const CSSToLengthConversionData&);

 private:
  LayoutObject* CreateLayoutObject(const ComputedStyle&,
                                   LegacyLayout legacy) override;
  bool IsPresentationAttribute(const QualifiedName&) const override;
  void CollectStyleForPresentationAttribute(
      const QualifiedName&,
      const AtomicString&,
      MutableCSSPropertyValueSet*) override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_MATHML_MATHML_SPACE_ELEMENT_H_
