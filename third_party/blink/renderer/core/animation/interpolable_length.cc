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

using UnitType = CSSPrimitiveValue::UnitType;

namespace {

CSSMathExpressionNode* NumberNode(double number) {
  return CSSMathExpressionNumericLiteral::Create(
      CSSNumericLiteralValue::Create(number, UnitType::kNumber));
}

CSSMathExpressionNode* PercentageNode(double number) {
  return CSSMathExpressionNumericLiteral::Create(
      CSSNumericLiteralValue::Create(number, UnitType::kPercentage));
}

}  // namespace

// static
std::unique_ptr<InterpolableLength> InterpolableLength::CreatePixels(
    double pixels) {
  return std::make_unique<InterpolableLength>(pixels, UnitType::kPixels);
}

// static
std::unique_ptr<InterpolableLength> InterpolableLength::CreatePercent(
    double percent) {
  return std::make_unique<InterpolableLength>(percent, UnitType::kPercentage);
}

// static
std::unique_ptr<InterpolableLength> InterpolableLength::CreateNeutral() {
  return std::make_unique<InterpolableLength>();
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

  if (const auto* numeric_literal =
          DynamicTo<CSSNumericLiteralValue>(primitive_value)) {
    return std::make_unique<InterpolableLength>(*numeric_literal);
  }

  return std::make_unique<InterpolableLength>(
      *To<CSSMathFunctionValue>(primitive_value)->ExpressionNode());
}

// static
std::unique_ptr<InterpolableLength> InterpolableLength::MaybeConvertLength(
    const Length& length,
    float zoom) {
  if (!length.IsSpecified())
    return nullptr;

  // Do not use CSSPrimitiveValue::CreateFromLength(), as it might drop 0% in
  // calculated lengths.
  // TODO(crbug.com/991672): Try not to drop 0% there.

  if (length.IsFixed())
    return CreatePixels(length.Pixels() / zoom);
  if (length.IsPercent())
    return CreatePercent(length.Percent());
  return std::make_unique<InterpolableLength>(
      *CSSMathExpressionNode::Create(length.GetCalculationValue()));
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
  if (start_length.HasPercentage() || end_length.HasPercentage()) {
    start_length.SetHasPercentage();
    end_length.SetHasPercentage();
  }
  return PairwiseInterpolationValue(std::move(start), std::move(end));
}

InterpolableLength::InterpolableLength(double value, UnitType unit_type) {
  SetNumericLiteral(value, unit_type);
}

InterpolableLength::InterpolableLength()
    : InterpolableLength(0, UnitType::kPixels) {}

InterpolableLength::InterpolableLength(const CSSNumericLiteralValue& value)
    : InterpolableLength(value.DoubleValue(), value.GetType()) {}

InterpolableLength::InterpolableLength(
    const CSSMathExpressionNode& expression) {
  SetExpression(expression);
}

std::unique_ptr<InterpolableValue> InterpolableLength::Clone() const {
  return std::make_unique<InterpolableLength>(*this);
}

void InterpolableLength::SetNumericLiteral(
    double value,
    CSSPrimitiveValue::UnitType unit_type) {
  type_ = Type::kNumericLiteral;
  single_value_ = value;
  unit_type_ = unit_type;
  expression_.Clear();
}

void InterpolableLength::SetNumericLiteral(
    const CSSNumericLiteralValue& value) {
  SetNumericLiteral(value.DoubleValue(), value.GetType());
}

void InterpolableLength::SetExpression(
    const CSSMathExpressionNode& expression) {
  if (expression.IsNumericLiteral()) {
    return SetNumericLiteral(
        *To<CSSMathExpressionNumericLiteral>(expression).GetValue());
  }

  type_ = Type::kExpression;
  expression_ = &expression;
}

const CSSMathExpressionNode& InterpolableLength::AsExpression() const {
  if (IsExpression())
    return *expression_;
  return *CSSMathExpressionNumericLiteral::Create(
      CSSNumericLiteralValue::Create(single_value_, unit_type_));
}

bool InterpolableLength::HasPercentage() const {
  if (IsNumericLiteral())
    return unit_type_ == UnitType::kPercentage;
  return expression_->HasPercentage();
}

void InterpolableLength::SetHasPercentage() {
  DEFINE_STATIC_LOCAL(Persistent<CSSMathExpressionNode>, zero_percent,
                      {PercentageNode(0)});
  if (HasPercentage())
    return;
  if (IsZeroLength())
    return SetNumericLiteral(0, UnitType::kPercentage);
  SetExpression(*CSSMathExpressionBinaryOperation::Create(
      &AsExpression(), zero_percent, CSSMathOperator::kAdd));
}

void InterpolableLength::SubtractFromOneHundredPercent() {
  DEFINE_STATIC_LOCAL(Persistent<CSSMathExpressionNode>, hundred_percent,
                      {PercentageNode(100)});
  if (IsNumericLiteral()) {
    if (unit_type_ == UnitType::kPercentage) {
      single_value_ = 100 - single_value_;
      return;
    }
    if (IsZeroLength())
      return SetNumericLiteral(100, UnitType::kPercentage);

    // Fall through, as the result requires an expression to represent
  }

  SetExpression(*CSSMathExpressionBinaryOperation::CreateSimplified(
      hundred_percent, &AsExpression(), CSSMathOperator::kSubtract));
}

static double ClampToRange(double x, ValueRange range) {
  return (range == kValueRangeNonNegative && x < 0) ? 0 : x;
}

Length InterpolableLength::CreateLength(
    const CSSToLengthConversionData& conversion_data,
    ValueRange range) const {
  if (IsNumericLiteral()) {
    const double value = ClampToRange(single_value_, range);
    if (CSSPrimitiveValue::IsLength(unit_type_)) {
      const double value_px =
          conversion_data.ZoomedComputedPixels(value, unit_type_);
      return Length::Fixed(CSSPrimitiveValue::ClampToCSSLengthRange(value_px));
    }
    DCHECK_EQ(UnitType::kPercentage, unit_type_);
    return Length::Percent(value);
  }

  // In the uncommon case where |this| is a math expression, this implementation
  // avoids a lot of code complexity at the cost of creating a temporary
  // |CSSMathFunctionValue| object.
  return CreateCSSValue(range)->ConvertToLength(conversion_data);
}

const CSSPrimitiveValue* InterpolableLength::CreateCSSValue(
    ValueRange range) const {
  if (IsExpression())
    return CSSMathFunctionValue::Create(&AsExpression(), range);

  DCHECK(IsNumericLiteral());
  return CSSNumericLiteralValue::Create(ClampToRange(single_value_, range),
                                        unit_type_);
}

void InterpolableLength::Scale(double scale) {
  if (IsNumericLiteral()) {
    single_value_ *= scale;
    return;
  }

  DCHECK(IsExpression());
  SetExpression(*CSSMathExpressionBinaryOperation::CreateSimplified(
      &AsExpression(), NumberNode(scale), CSSMathOperator::kMultiply));
}

void InterpolableLength::ScaleAndAdd(double scale,
                                     const InterpolableValue& other) {
  const InterpolableLength& other_length = To<InterpolableLength>(other);
  if (IsNumericLiteral() && other_length.IsNumericLiteral() &&
      unit_type_ == other_length.unit_type_) {
    single_value_ = single_value_ * scale + other_length.single_value_;
    return;
  }

  // Avoid creating an addition expression with a zero-length operand.
  if (IsZeroLength()) {
    *this = other_length;
    return;
  }
  if (other_length.IsZeroLength())
    return Scale(scale);

  CSSMathExpressionNode* scaled =
      CSSMathExpressionBinaryOperation::CreateSimplified(
          &AsExpression(), NumberNode(scale), CSSMathOperator::kMultiply);
  CSSMathExpressionNode* result =
      CSSMathExpressionBinaryOperation::CreateSimplified(
          scaled, &other_length.AsExpression(), CSSMathOperator::kAdd);
  SetExpression(*result);
}

void InterpolableLength::AssertCanInterpolateWith(
    const InterpolableValue& other) const {
  DCHECK(other.IsLength());
  // TODO(crbug.com/991672): Ensure that all |MergeSingles| variants that merge
  // two |InterpolableLength| objects should also assign them the same shape
  // (i.e. type flags) after merging into a |PairwiseInterpolationValue|. We
  // currently fail to do that, and hit the following DCHECK:
  // DCHECK_EQ(HasPercentage(),
  //           To<InterpolableLength>(other).HasPercentage());
}

void InterpolableLength::Interpolate(const InterpolableValue& to,
                                     const double progress,
                                     InterpolableValue& result) const {
  const auto& to_length = To<InterpolableLength>(to);
  auto& result_length = To<InterpolableLength>(result);

  if (IsNumericLiteral() && to_length.IsNumericLiteral() &&
      unit_type_ == to_length.unit_type_) {
    return result_length.SetNumericLiteral(
        Blend(single_value_, to_length.single_value_, progress), unit_type_);
  }

  // Avoid creating an addition expression with a zero-length operand.
  if (IsZeroLength()) {
    result_length = to_length;
    return result_length.Scale(progress);
  }
  if (to_length.IsZeroLength()) {
    result_length = *this;
    return result_length.Scale(1 - progress);
  }

  CSSMathExpressionNode* blended_from =
      CSSMathExpressionBinaryOperation::CreateSimplified(
          &AsExpression(), NumberNode(1 - progress),
          CSSMathOperator::kMultiply);
  CSSMathExpressionNode* blended_to =
      CSSMathExpressionBinaryOperation::CreateSimplified(
          &to_length.AsExpression(), NumberNode(progress),
          CSSMathOperator::kMultiply);
  CSSMathExpressionNode* result_expression =
      CSSMathExpressionBinaryOperation::CreateSimplified(
          blended_from, blended_to, CSSMathOperator::kAdd);
  result_length.SetExpression(*result_expression);

  DCHECK_EQ(result_length.HasPercentage(),
            HasPercentage() || to_length.HasPercentage());
}

}  // namespace blink
