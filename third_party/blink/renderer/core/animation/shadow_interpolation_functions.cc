// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/shadow_interpolation_functions.h"

#include <memory>
#include "third_party/blink/renderer/core/animation/css_color_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/interpolable_length.h"
#include "third_party/blink/renderer/core/animation/interpolation_value.h"
#include "third_party/blink/renderer/core/animation/non_interpolable_value.h"
#include "third_party/blink/renderer/core/animation/underlying_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_shadow_value.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/shadow_data.h"
#include "third_party/blink/renderer/platform/geometry/float_point.h"

namespace blink {

enum ShadowComponentIndex : unsigned {
  kShadowX,
  kShadowY,
  kShadowBlur,
  kShadowSpread,
  kShadowColor,
  kShadowComponentIndexCount,
};

class ShadowNonInterpolableValue : public NonInterpolableValue {
 public:
  ~ShadowNonInterpolableValue() final = default;

  static scoped_refptr<ShadowNonInterpolableValue> Create(
      ShadowStyle shadow_style) {
    return base::AdoptRef(new ShadowNonInterpolableValue(shadow_style));
  }

  ShadowStyle Style() const { return style_; }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  ShadowNonInterpolableValue(ShadowStyle shadow_style) : style_(shadow_style) {}

  ShadowStyle style_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(ShadowNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(ShadowNonInterpolableValue);

bool ShadowInterpolationFunctions::NonInterpolableValuesAreCompatible(
    const NonInterpolableValue* a,
    const NonInterpolableValue* b) {
  return ToShadowNonInterpolableValue(*a).Style() ==
         ToShadowNonInterpolableValue(*b).Style();
}

PairwiseInterpolationValue ShadowInterpolationFunctions::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) {
  if (!NonInterpolableValuesAreCompatible(start.non_interpolable_value.get(),
                                          end.non_interpolable_value.get()))
    return nullptr;
  return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                    std::move(end.interpolable_value),
                                    std::move(start.non_interpolable_value));
}

InterpolationValue ShadowInterpolationFunctions::ConvertShadowData(
    const ShadowData& shadow_data,
    double zoom) {
  auto interpolable_list =
      std::make_unique<InterpolableList>(kShadowComponentIndexCount);
  interpolable_list->Set(
      kShadowX, InterpolableLength::CreatePixels(shadow_data.X() / zoom));
  interpolable_list->Set(
      kShadowY, InterpolableLength::CreatePixels(shadow_data.Y() / zoom));
  interpolable_list->Set(
      kShadowBlur, InterpolableLength::CreatePixels(shadow_data.Blur() / zoom));
  interpolable_list->Set(kShadowSpread, InterpolableLength::CreatePixels(
                                            shadow_data.Spread() / zoom));
  interpolable_list->Set(kShadowColor,
                         CSSColorInterpolationType::CreateInterpolableColor(
                             shadow_data.GetColor()));
  return InterpolationValue(
      std::move(interpolable_list),
      ShadowNonInterpolableValue::Create(shadow_data.Style()));
}

InterpolationValue ShadowInterpolationFunctions::MaybeConvertCSSValue(
    const CSSValue& value) {
  const auto* shadow = DynamicTo<CSSShadowValue>(value);
  if (!shadow)
    return nullptr;

  ShadowStyle style = kNormal;
  if (shadow->style) {
    if (shadow->style->GetValueID() == CSSValueID::kInset)
      style = kInset;
    else
      return nullptr;
  }

  auto interpolable_list =
      std::make_unique<InterpolableList>(kShadowComponentIndexCount);
  static_assert(kShadowX == 0, "Enum ordering check.");
  static_assert(kShadowY == 1, "Enum ordering check.");
  static_assert(kShadowBlur == 2, "Enum ordering check.");
  static_assert(kShadowSpread == 3, "Enum ordering check.");
  const CSSPrimitiveValue* lengths[] = {
      shadow->x.Get(),
      shadow->y.Get(),
      shadow->blur.Get(),
      shadow->spread.Get(),
  };
  for (wtf_size_t i = 0; i < base::size(lengths); i++) {
    if (lengths[i]) {
      InterpolationValue length_field(
          InterpolableLength::MaybeConvertCSSValue(*lengths[i]));
      if (!length_field)
        return nullptr;
      DCHECK(!length_field.non_interpolable_value);
      interpolable_list->Set(i, std::move(length_field.interpolable_value));
    } else {
      interpolable_list->Set(i, InterpolableLength::CreatePixels(0));
    }
  }

  if (shadow->color) {
    std::unique_ptr<InterpolableValue> interpolable_color =
        CSSColorInterpolationType::MaybeCreateInterpolableColor(*shadow->color);
    if (!interpolable_color)
      return nullptr;
    interpolable_list->Set(kShadowColor, std::move(interpolable_color));
  } else {
    interpolable_list->Set(kShadowColor,
                           CSSColorInterpolationType::CreateInterpolableColor(
                               StyleColor::CurrentColor()));
  }

  return InterpolationValue(std::move(interpolable_list),
                            ShadowNonInterpolableValue::Create(style));
}

std::unique_ptr<InterpolableValue>
ShadowInterpolationFunctions::CreateNeutralInterpolableValue() {
  return ConvertShadowData(ShadowData::NeutralValue(), 1).interpolable_value;
}

void ShadowInterpolationFunctions::Composite(
    UnderlyingValue& underlying_value,
    double underlying_fraction,
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value) {
  DCHECK(NonInterpolableValuesAreCompatible(
      underlying_value.GetNonInterpolableValue(), non_interpolable_value));
  InterpolableList& underlying_interpolable_list =
      ToInterpolableList(underlying_value.MutableInterpolableValue());
  const InterpolableList& interpolable_list =
      ToInterpolableList(interpolable_value);
  underlying_interpolable_list.ScaleAndAdd(underlying_fraction,
                                           interpolable_list);
}

ShadowData ShadowInterpolationFunctions::CreateShadowData(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    const StyleResolverState& state) {
  const InterpolableList& interpolable_list =
      ToInterpolableList(interpolable_value);
  const ShadowNonInterpolableValue& shadow_non_interpolable_value =
      ToShadowNonInterpolableValue(*non_interpolable_value);
  const CSSToLengthConversionData& conversion_data =
      state.CssToLengthConversionData();
  Length shadow_x = To<InterpolableLength>(*interpolable_list.Get(kShadowX))
                        .CreateLength(conversion_data, kValueRangeAll);
  Length shadow_y = To<InterpolableLength>(*interpolable_list.Get(kShadowY))
                        .CreateLength(conversion_data, kValueRangeAll);
  Length shadow_blur =
      To<InterpolableLength>(*interpolable_list.Get(kShadowBlur))
          .CreateLength(conversion_data, kValueRangeNonNegative);
  Length shadow_spread =
      To<InterpolableLength>(*interpolable_list.Get(kShadowSpread))
          .CreateLength(conversion_data, kValueRangeAll);
  DCHECK(shadow_x.IsFixed() && shadow_y.IsFixed() && shadow_blur.IsFixed() &&
         shadow_spread.IsFixed());
  return ShadowData(FloatPoint(shadow_x.Value(), shadow_y.Value()),
                    shadow_blur.Value(), shadow_spread.Value(),
                    shadow_non_interpolable_value.Style(),
                    CSSColorInterpolationType::ResolveInterpolableColor(
                        *interpolable_list.Get(kShadowColor), state));
}

}  // namespace blink
