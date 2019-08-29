// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_custom_length_interpolation_type.h"

#include "third_party/blink/renderer/core/animation/length_interpolation_functions.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"

namespace blink {

InterpolationValue CSSCustomLengthInterpolationType::MaybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  return InterpolationValue(
      LengthInterpolationFunctions::CreateNeutralInterpolableValue());
}

InterpolationValue CSSCustomLengthInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  InterpolationValue interpolation_value =
      LengthInterpolationFunctions::MaybeConvertCSSValue(value);
  if (LengthInterpolationFunctions::HasPercentage(
          *interpolation_value.interpolable_value))
    return nullptr;
  return interpolation_value;
}

const CSSValue* CSSCustomLengthInterpolationType::CreateCSSValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    const StyleResolverState&) const {
  DCHECK(!LengthInterpolationFunctions::HasPercentage(interpolable_value));
  return LengthInterpolationFunctions::CreateCSSValue(
      interpolable_value, non_interpolable_value, kValueRangeAll);
}

}  // namespace blink
