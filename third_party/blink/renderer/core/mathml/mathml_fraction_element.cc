// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mathml/mathml_fraction_element.h"

#include "third_party/blink/renderer/core/layout/ng/mathml/layout_ng_mathml_block.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

MathMLFractionElement::MathMLFractionElement(Document& doc)
    : MathMLElement(mathml_names::kMfracTag, doc) {}

void MathMLFractionElement::AddMathFractionBarThicknessIfNeeded(
    ComputedStyle& style,
    const CSSToLengthConversionData& conversion_data) {
  if (auto length_or_percentage_value = AddMathLengthToComputedStyle(
          style, conversion_data, mathml_names::kLinethicknessAttr))
    style.SetMathFractionBarThickness(std::move(*length_or_percentage_value));
}

LayoutObject* MathMLFractionElement::CreateLayoutObject(
    const ComputedStyle& style,
    LegacyLayout legacy) {
  if (!RuntimeEnabledFeatures::MathMLCoreEnabled() ||
      !style.IsDisplayMathType() || legacy == LegacyLayout::kForce)
    return MathMLElement::CreateLayoutObject(style, legacy);
  return new LayoutNGMathMLBlock(this);
}

}  // namespace blink
