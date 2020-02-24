// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mathml/mathml_space_element.h"

#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/layout/ng/mathml/layout_ng_mathml_block.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

MathMLSpaceElement::MathMLSpaceElement(Document& doc)
    : MathMLElement(mathml_names::kMspaceTag, doc) {}

void MathMLSpaceElement::AddMathBaselineIfNeeded(
    ComputedStyle& style,
    const CSSToLengthConversionData& conversion_data) {
  if (!FastHasAttribute(mathml_names::kHeightAttr))
    return;
  auto string = FastGetAttribute(mathml_names::kHeightAttr);
  const CSSValue* parsed = CSSParser::ParseSingleValue(
      CSSPropertyID::kHeight, string,
      StrictCSSParserContext(GetDocument().GetSecureContextMode()));
  const auto* new_value = DynamicTo<CSSPrimitiveValue>(parsed);
  if (!new_value || !new_value->IsLength())
    return;
  Length length_or_percentage_value =
      new_value->ConvertToLength(conversion_data);
  style.SetMathBaseline(std::move(length_or_percentage_value));
}

bool MathMLSpaceElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  if (name == mathml_names::kWidthAttr || name == mathml_names::kHeightAttr ||
      name == mathml_names::kDepthAttr)
    return true;
  return MathMLElement::IsPresentationAttribute(name);
}

void MathMLSpaceElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name == mathml_names::kWidthAttr) {
    AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kWidth,
                                            value);
  } else if (name == mathml_names::kHeightAttr ||
             name == mathml_names::kDepthAttr) {
    // TODO(rbuis): this can be simplified once attr() is supported for
    // width/height.
    String height = FastGetAttribute(mathml_names::kHeightAttr);
    String depth = FastGetAttribute(mathml_names::kDepthAttr);
    if (!height.IsEmpty() && !depth.IsEmpty()) {
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kHeight,
          "calc(" + height + " + " + depth + ")");
    } else {
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kHeight,
                                              value);
    }
  } else {
    MathMLElement::CollectStyleForPresentationAttribute(name, value, style);
  }
}

LayoutObject* MathMLSpaceElement::CreateLayoutObject(const ComputedStyle& style,
                                                     LegacyLayout legacy) {
  if (!RuntimeEnabledFeatures::MathMLCoreEnabled() ||
      !style.IsDisplayMathType() || legacy == LegacyLayout::kForce)
    return MathMLElement::CreateLayoutObject(style, legacy);
  return new LayoutNGMathMLBlock(this);
}

}  // namespace blink
