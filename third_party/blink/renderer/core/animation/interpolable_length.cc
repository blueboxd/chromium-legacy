// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/interpolable_length.h"

#include "third_party/blink/renderer/core/animation/underlying_value.h"
#include "third_party/blink/renderer/core/css/css_math_expression_node.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/platform/geometry/blend.h"
#include "third_party/blink/renderer/platform/geometry/calculation_value.h"

namespace blink {

// static
std::unique_ptr<InterpolableLength> InterpolableLength::CreatePixels(
    double pixels) {
  CSSLengthArray length_array;
  length_array.values[CSSPrimitiveValue::kUnitTypePixels] = pixels;
  length_array.type_flags.set(CSSPrimitiveValue::kUnitTypePixels);
  return std::make_unique<InterpolableLength>(std::move(length_array));
}

// static
std::unique_ptr<InterpolableLength> InterpolableLength::CreatePercent(
    double percent) {
  CSSLengthArray length_array;
  length_array.values[CSSPrimitiveValue::kUnitTypePercentage] = percent;
  length_array.type_flags.set(CSSPrimitiveValue::kUnitTypePercentage);
  return std::make_unique<InterpolableLength>(std::move(length_array));
}

// static
std::unique_ptr<InterpolableLength> InterpolableLength::CreateNeutral() {
  return std::make_unique<InterpolableLength>(CSSLengthArray());
}

// static
std::unique_ptr<InterpolableLength> InterpolableLength::MaybeConvertCSSValue(
    const CSSValue& value) {
  const auto* primitive_value = DynamicTo<CSSPrimitiveValue>(value);
  if (!primitive_value)
    return nullptr;

  if (!primitive_value->IsLength() && !primitive_value->IsPercentage() &&
      !primitive_value->IsCalculatedPercentageWithLength())
    return nullptr;

  CSSLengthArray length_array;
  if (!primitive_value->AccumulateLengthArray(length_array)) {
    // TODO(crbug.com/991672): Implement interpolation when CSS comparison
    // functions min/max are involved.
    return nullptr;
  }

  return std::make_unique<InterpolableLength>(std::move(length_array));
}

// static
std::unique_ptr<InterpolableLength> InterpolableLength::MaybeConvertLength(
    const Length& length,
    float zoom) {
  if (!length.IsSpecified())
    return nullptr;

  if (length.IsCalculated() && length.GetCalculationValue().IsExpression()) {
    // TODO(crbug.com/991672): Support interpolation on min/max results.
    return nullptr;
  }

  PixelsAndPercent pixels_and_percent = length.GetPixelsAndPercent();
  CSSLengthArray length_array;

  length_array.values[CSSPrimitiveValue::kUnitTypePixels] =
      pixels_and_percent.pixels / zoom;
  length_array.type_flags[CSSPrimitiveValue::kUnitTypePixels] =
      pixels_and_percent.pixels != 0;

  length_array.values[CSSPrimitiveValue::kUnitTypePercentage] =
      pixels_and_percent.percent;
  length_array.type_flags[CSSPrimitiveValue::kUnitTypePercentage] =
      length.IsPercentOrCalc();
  return std::make_unique<InterpolableLength>(std::move(length_array));
}

// static
PairwiseInterpolationValue InterpolableLength::MergeSingles(
    std::unique_ptr<InterpolableValue> start,
    std::unique_ptr<InterpolableValue> end) {
  // TODO(crbug.com/991672): We currently have a lot of "fast paths" that do not
  // go through here, and hence, do not merge the type flags of two lengths. We
  // should stop doing that.
  auto& start_length = To<InterpolableLength>(*start);
  auto& end_length = To<InterpolableLength>(*end);
  start_length.length_array_.type_flags |= end_length.length_array_.type_flags;
  end_length.length_array_.type_flags = start_length.length_array_.type_flags;
  return PairwiseInterpolationValue(std::move(start), std::move(end));
}

void InterpolableLength::SubtractFromOneHundredPercent() {
  for (double& value : length_array_.values)
    value *= -1;
  length_array_.values[CSSPrimitiveValue::kUnitTypePercentage] += 100;
  length_array_.type_flags.set(CSSPrimitiveValue::kUnitTypePercentage);
}

static double ClampToRange(double x, ValueRange range) {
  return (range == kValueRangeNonNegative && x < 0) ? 0 : x;
}

static CSSPrimitiveValue::UnitType IndexToUnitType(wtf_size_t index) {
  return CSSPrimitiveValue::LengthUnitTypeToUnitType(
      static_cast<CSSPrimitiveValue::LengthUnitType>(index));
}

Length InterpolableLength::CreateLength(
    const CSSToLengthConversionData& conversion_data,
    ValueRange range) const {
  bool has_percentage = HasPercentage();
  double pixels = 0;
  double percentage = 0;
  for (wtf_size_t i = 0; i < length_array_.values.size(); ++i) {
    double value = length_array_.values[i];
    if (value == 0)
      continue;
    if (i == CSSPrimitiveValue::kUnitTypePercentage) {
      percentage = value;
    } else {
      pixels += conversion_data.ZoomedComputedPixels(value, IndexToUnitType(i));
    }
  }

  if (percentage != 0)
    has_percentage = true;
  if (pixels != 0 && has_percentage) {
    return Length(CalculationValue::Create(
        PixelsAndPercent(clampTo<float>(pixels), clampTo<float>(percentage)),
        range));
  }
  if (has_percentage)
    return Length::Percent(ClampToRange(percentage, range));
  return Length::Fixed(
      CSSPrimitiveValue::ClampToCSSLengthRange(ClampToRange(pixels, range)));
}

const CSSPrimitiveValue* InterpolableLength::CreateCSSValue(
    ValueRange range) const {
  bool has_percentage = HasPercentage();

  CSSMathExpressionNode* root_node = nullptr;
  CSSNumericLiteralValue* first_value = nullptr;

  for (wtf_size_t i = 0; i < length_array_.values.size(); ++i) {
    double value = length_array_.values[i];
    if (value == 0 &&
        (i != CSSPrimitiveValue::kUnitTypePercentage || !has_percentage)) {
      continue;
    }
    CSSNumericLiteralValue* current_value =
        CSSNumericLiteralValue::Create(value, IndexToUnitType(i));

    if (!first_value) {
      DCHECK(!root_node);
      first_value = current_value;
      continue;
    }
    CSSMathExpressionNode* current_node =
        CSSMathExpressionNumericLiteral::Create(current_value);
    if (!root_node) {
      root_node = CSSMathExpressionNumericLiteral::Create(first_value);
    }
    root_node = CSSMathExpressionBinaryOperation::Create(
        root_node, current_node, CSSMathOperator::kAdd);
  }

  if (root_node) {
    return CSSMathFunctionValue::Create(root_node, range);
  }
  if (first_value) {
    if (range == kValueRangeNonNegative && first_value->DoubleValue() < 0)
      return CSSNumericLiteralValue::Create(0, first_value->GetType());
    return first_value;
  }
  return CSSNumericLiteralValue::Create(0,
                                        CSSPrimitiveValue::UnitType::kPixels);
}

void InterpolableLength::ScaleAndAdd(double scale,
                                     const InterpolableValue& other) {
  const InterpolableLength& other_length = To<InterpolableLength>(other);
  for (wtf_size_t i = 0; i < length_array_.values.size(); ++i) {
    length_array_.values[i] =
        length_array_.values[i] * scale + other_length.length_array_.values[i];
  }
  length_array_.type_flags |= other_length.length_array_.type_flags;
}

void InterpolableLength::AssertCanInterpolateWith(
    const InterpolableValue& other) const {
  DCHECK(other.IsLength());
  // TODO(crbug.com/991672): Ensure that all |MergeSingles| variants that merge
  // two |InterpolableLength| objects should also assign them the same shape
  // (i.e. type flags) after merging into a |PairwiseInterpolationValue|. We
  // currently fail to do that, and hit the following DCHECK:
  // DCHECK_EQ(length_array_.type_flags,
  //           To<InterpolableLength>(other).length_array_.type_flags);
}

void InterpolableLength::Interpolate(const InterpolableValue& to,
                                     const double progress,
                                     InterpolableValue& result) const {
  const CSSLengthArray& to_length_array =
      To<InterpolableLength>(to).length_array_;
  CSSLengthArray& result_length_array =
      To<InterpolableLength>(result).length_array_;
  for (wtf_size_t i = 0; i < length_array_.values.size(); ++i) {
    result_length_array.values[i] =
        Blend(length_array_.values[i], to_length_array.values[i], progress);
  }
  result_length_array.type_flags =
      length_array_.type_flags | to_length_array.type_flags;
}

}  // namespace blink
