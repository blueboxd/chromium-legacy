// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mathml/mathml_under_over_element.h"

#include "third_party/blink/renderer/core/layout/ng/mathml/layout_ng_mathml_block.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

static MathMLUnderOverElement::ScriptType scriptTypeOf(
    const QualifiedName& tagName) {
  if (tagName == mathml_names::kMunderTag)
    return MathMLUnderOverElement::ScriptType::kUnder;
  if (tagName == mathml_names::kMoverTag)
    return MathMLUnderOverElement::ScriptType::kOver;
  DCHECK_EQ(tagName, mathml_names::kMunderoverTag);
  return MathMLUnderOverElement::ScriptType::kUnderOver;
}

MathMLUnderOverElement::MathMLUnderOverElement(const QualifiedName& tagName,
                                               Document& document)
    : MathMLElement(tagName, document), script_type_(scriptTypeOf(tagName)) {}

LayoutObject* MathMLUnderOverElement::CreateLayoutObject(
    const ComputedStyle& style,
    LegacyLayout legacy) {
  if (!RuntimeEnabledFeatures::MathMLCoreEnabled() ||
      !style.IsDisplayMathType() || legacy == LegacyLayout::kForce)
    return MathMLElement::CreateLayoutObject(style, legacy);
  return new LayoutNGMathMLBlock(this);
}

}  // namespace blink
