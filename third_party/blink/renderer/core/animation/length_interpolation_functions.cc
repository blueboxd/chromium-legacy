// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/length_interpolation_functions.h"

#include "third_party/blink/renderer/core/css/css_math_expression_node.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/platform/geometry/calculation_value.h"

namespace blink {

// This class is implemented as a singleton whose instance represents the
// presence of percentages being used in a Length value while nullptr represents
// the absence of any percentages.
class CSSLengthNonInterpolableValue : public NonInterpolableValue {
 public:
  ~CSSLengthNonInterpolableValue() final { NOTREACHED(); }
  static scoped_refptr<CSSLengthNonInterpolableValue> Create(
      bool has_percentage) {
    DEFINE_STATIC_REF(CSSLengthNonInterpolableValue, singleton,
                      base::AdoptRef(new CSSLengthNonInterpolableValue()));
    DCHECK(singleton);
    return has_percentage ? singleton : nullptr;
  }
  static scoped_refptr<CSSLengthNonInterpolableValue> Merge(
      const NonInterpolableValue* a,
      const NonInterpolableValue* b) {
    return Create(HasPercentage(a) || HasPercentage(b));
  }
  static bool HasPercentage(const NonInterpolableValue*);

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  CSSLengthNonInterpolableValue() = default;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSLengthNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(CSSLengthNonInterpolableValue);

bool CSSLengthNonInterpolableValue::HasPercentage(
    const NonInterpolableValue* non_interpolable_value) {
  DCHECK(IsCSSLengthNonInterpolableValue(non_interpolable_value));
  return static_cast<bool>(non_interpolable_value);
}

std::unique_ptr<InterpolableValue>
LengthInterpolationFunctions::CreateInterpolablePixels(double pixels) {
  auto interpolable_list = CreateNeutralInterpolableValue();
  interpolable_list->Set(CSSPrimitiveValue::kUnitTypePixels,
                         std::make_unique<InterpolableNumber>(pixels));
  return std::move(interpolable_list);
}

InterpolationValue LengthInterpolationFunctions::CreateInterpolablePercent(
    double percent) {
  auto interpolable_list = CreateNeutralInterpolableValue();
  interpolable_list->Set(CSSPrimitiveValue::kUnitTypePercentage,
                         std::make_unique<InterpolableNumber>(percent));
  return InterpolationValue(std::move(interpolable_list),
                            CSSLengthNonInterpolableValue::Create(true));
}

std::unique_ptr<InterpolableList>
LengthInterpolationFunctions::CreateNeutralInterpolableValue() {
  const size_t kLength = CSSPrimitiveValue::kLengthUnitTypeCount;
  auto values = std::make_unique<InterpolableList>(kLength);
  for (wtf_size_t i = 0; i < kLength; i++)
    values->Set(i, std::make_unique<InterpolableNumber>(0));
  return values;
}

InterpolationValue LengthInterpolationFunctions::MaybeConvertCSSValue(
    const CSSValue& value) {
  const auto* primitive_value = DynamicTo<CSSPrimitiveValue>(value);
  if (!primitive_value)
    return nullptr;

  if (!primitive_value->IsLength() && !primitive_value->IsPercentage() &&
      !primitive_value->IsCalculatedPercentageWithLength())
    return nullptr;

  CSSLengthArray length_array;
  primitive_value->AccumulateLengthArray(length_array);

  auto values = std::make_unique<InterpolableList>(
      CSSPrimitiveValue::kLengthUnitTypeCount);
  for (wtf_size_t i = 0; i < CSSPrimitiveValue::kLengthUnitTypeCount; i++) {
    values->Set(i,
                std::make_unique<InterpolableNumber>(length_array.values[i]));
  }

  bool has_percentage =
      length_array.type_flags[CSSPrimitiveValue::kUnitTypePercentage];
  return InterpolationValue(
      std::move(values), CSSLengthNonInterpolableValue::Create(has_percentage));
}

InterpolationValue LengthInterpolationFunctions::MaybeConvertLength(
    const Length& length,
    float zoom) {
  if (!length.IsSpecified())
    return nullptr;

  PixelsAndPercent pixels_and_percent = length.GetPixelsAndPercent();
  auto values = CreateNeutralInterpolableValue();
  values->Set(
      CSSPrimitiveValue::kUnitTypePixels,
      std::make_unique<InterpolableNumber>(pixels_and_percent.pixels / zoom));
  values->Set(CSSPrimitiveValue::kUnitTypePercentage,
              std::make_unique<InterpolableNumber>(pixels_and_percent.percent));

  return InterpolationValue(
      std::move(values),
      CSSLengthNonInterpolableValue::Create(length.IsPercentOrCalc()));
}

PairwiseInterpolationValue LengthInterpolationFunctions::MergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) {
  return PairwiseInterpolationValue(
      std::move(start.interpolable_value), std::move(end.interpolable_value),
      CSSLengthNonInterpolableValue::Merge(start.non_interpolable_value.get(),
                                           end.non_interpolable_value.get()));
}

bool LengthInterpolationFunctions::NonInterpolableValuesAreCompatible(
    const NonInterpolableValue* a,
    const NonInterpolableValue* b) {
  DCHECK(IsCSSLengthNonInterpolableValue(a));
  DCHECK(IsCSSLengthNonInterpolableValue(b));
  return true;
}

bool LengthInterpolationFunctions::HasPercentage(
    const NonInterpolableValue* non_interpolable_value) {
  return CSSLengthNonInterpolableValue::HasPercentage(non_interpolable_value);
}

void LengthInterpolationFunctions::Composite(
    std::unique_ptr<InterpolableValue>& underlying_interpolable_value,
    scoped_refptr<NonInterpolableValue>& underlying_non_interpolable_value,
    double underlying_fraction,
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value) {
  underlying_interpolable_value->ScaleAndAdd(underlying_fraction,
                                             interpolable_value);
  underlying_non_interpolable_value = CSSLengthNonInterpolableValue::Merge(
      underlying_non_interpolable_value.get(), non_interpolable_value);
}

void LengthInterpolationFunctions::SubtractFromOneHundredPercent(
    InterpolationValue& result) {
  InterpolableList& list = ToInterpolableList(*result.interpolable_value);
  for (wtf_size_t i = 0; i < CSSPrimitiveValue::kLengthUnitTypeCount; i++) {
    double value = -ToInterpolableNumber(*list.Get(i)).Value();
    if (i == CSSPrimitiveValue::kUnitTypePercentage)
      value += 100;
    ToInterpolableNumber(*list.GetMutable(i)).Set(value);
  }
  result.non_interpolable_value = CSSLengthNonInterpolableValue::Create(true);
}

static double ClampToRange(double x, ValueRange range) {
  return (range == kValueRangeNonNegative && x < 0) ? 0 : x;
}

CSSPrimitiveValue::UnitType IndexToUnitType(size_t index) {
  return CSSPrimitiveValue::LengthUnitTypeToUnitType(
      static_cast<CSSPrimitiveValue::LengthUnitType>(index));
}

Length LengthInterpolationFunctions::CreateLength(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    const CSSToLengthConversionData& conversion_data,
    ValueRange range) {
  const InterpolableList& interpolable_list =
      ToInterpolableList(interpolable_value);
  bool has_percentage =
      CSSLengthNonInterpolableValue::HasPercentage(non_interpolable_value);
  double pixels = 0;
  double percentage = 0;
  for (wtf_size_t i = 0; i < CSSPrimitiveValue::kLengthUnitTypeCount; i++) {
    double value = ToInterpolableNumber(*interpolable_list.Get(i)).Value();
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

const CSSValue* LengthInterpolationFunctions::CreateCSSValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    ValueRange range) {
  const InterpolableList& interpolable_list =
      ToInterpolableList(interpolable_value);
  bool has_percentage =
      CSSLengthNonInterpolableValue::HasPercentage(non_interpolable_value);

  CSSMathExpressionNode* root_node = nullptr;
  CSSNumericLiteralValue* first_value = nullptr;

  for (wtf_size_t i = 0; i < CSSPrimitiveValue::kLengthUnitTypeCount; i++) {
    double value = ToInterpolableNumber(*interpolable_list.Get(i)).Value();
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
    return CSSMathFunctionValue::Create(root_node);
  }
  if (first_value) {
    return first_value;
  }
  return CSSNumericLiteralValue::Create(0,
                                        CSSPrimitiveValue::UnitType::kPixels);
}

}  // namespace blink
