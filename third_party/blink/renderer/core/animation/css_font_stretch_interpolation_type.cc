// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_font_stretch_interpolation_type.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/css/css_primitive_value_mappings.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

class InheritedFontStretchChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  explicit InheritedFontStretchChecker(FontSelectionValue font_stretch)
      : font_stretch_(font_stretch) {}

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue&) const final {
    return font_stretch_ == state.ParentStyle()->GetFontStretch();
  }

  const double font_stretch_;
};

InterpolationValue CSSFontStretchInterpolationType::CreateFontStretchValue(
    FontSelectionValue font_stretch) const {
  return InterpolationValue(std::make_unique<InterpolableNumber>(font_stretch));
}

InterpolationValue CSSFontStretchInterpolationType::MaybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  return InterpolationValue(std::make_unique<InterpolableNumber>(0));
}

InterpolationValue CSSFontStretchInterpolationType::MaybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers& conversion_checkers) const {
  return CreateFontStretchValue(NormalWidthValue());
}

InterpolationValue CSSFontStretchInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  if (!state.ParentStyle())
    return nullptr;
  FontSelectionValue inherited_font_stretch =
      state.ParentStyle()->GetFontStretch();
  conversion_checkers.push_back(
      std::make_unique<InheritedFontStretchChecker>(inherited_font_stretch));
  return CreateFontStretchValue(inherited_font_stretch);
}

InterpolationValue CSSFontStretchInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState* state,
    ConversionCheckers& conversion_checkers) const {
  if (auto* primitive_value = DynamicTo<CSSPrimitiveValue>(value)) {
    return CreateFontStretchValue(
        FontSelectionValue(primitive_value->GetFloatValue()));
  }

  const auto& identifier_value = To<CSSIdentifierValue>(value);
  CSSValueID keyword = identifier_value.GetValueID();

  switch (keyword) {
    case CSSValueID::kInvalid:
      return nullptr;
    case CSSValueID::kUltraCondensed:
      return CreateFontStretchValue(UltraCondensedWidthValue());
    case CSSValueID::kExtraCondensed:
      return CreateFontStretchValue(ExtraCondensedWidthValue());
    case CSSValueID::kCondensed:
      return CreateFontStretchValue(CondensedWidthValue());
    case CSSValueID::kSemiCondensed:
      return CreateFontStretchValue(SemiCondensedWidthValue());
    case CSSValueID::kNormal:
      return CreateFontStretchValue(NormalWidthValue());
    case CSSValueID::kSemiExpanded:
      return CreateFontStretchValue(SemiExpandedWidthValue());
    case CSSValueID::kExpanded:
      return CreateFontStretchValue(ExpandedWidthValue());
    case CSSValueID::kExtraExpanded:
      return CreateFontStretchValue(ExtraExpandedWidthValue());
    case CSSValueID::kUltraExpanded:
      return CreateFontStretchValue(UltraExpandedWidthValue());
    default:
      NOTREACHED();
      return nullptr;
  }
}

InterpolationValue
CSSFontStretchInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  return CreateFontStretchValue(style.GetFontStretch());
}

void CSSFontStretchInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue*,
    StyleResolverState& state) const {
  state.GetFontBuilder().SetStretch(FontSelectionValue(
      clampTo(To<InterpolableNumber>(interpolable_value).Value(), 0.0)));
}

}  // namespace blink
