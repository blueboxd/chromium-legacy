/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/css/css_calculation_value.h"

#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value_mappings.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

static const int maxExpressionDepth = 100;

enum ParseState { OK, TooDeep, NoMoreTokens };

namespace blink {

static CalculationCategory UnitCategory(CSSPrimitiveValue::UnitType type) {
  switch (type) {
    case CSSPrimitiveValue::UnitType::kNumber:
    case CSSPrimitiveValue::UnitType::kInteger:
      return kCalcNumber;
    case CSSPrimitiveValue::UnitType::kPercentage:
      return kCalcPercent;
    case CSSPrimitiveValue::UnitType::kEms:
    case CSSPrimitiveValue::UnitType::kExs:
    case CSSPrimitiveValue::UnitType::kPixels:
    case CSSPrimitiveValue::UnitType::kCentimeters:
    case CSSPrimitiveValue::UnitType::kMillimeters:
    case CSSPrimitiveValue::UnitType::kQuarterMillimeters:
    case CSSPrimitiveValue::UnitType::kInches:
    case CSSPrimitiveValue::UnitType::kPoints:
    case CSSPrimitiveValue::UnitType::kPicas:
    case CSSPrimitiveValue::UnitType::kUserUnits:
    case CSSPrimitiveValue::UnitType::kRems:
    case CSSPrimitiveValue::UnitType::kChs:
    case CSSPrimitiveValue::UnitType::kViewportWidth:
    case CSSPrimitiveValue::UnitType::kViewportHeight:
    case CSSPrimitiveValue::UnitType::kViewportMin:
    case CSSPrimitiveValue::UnitType::kViewportMax:
      return kCalcLength;
    case CSSPrimitiveValue::UnitType::kDegrees:
    case CSSPrimitiveValue::UnitType::kGradians:
    case CSSPrimitiveValue::UnitType::kRadians:
    case CSSPrimitiveValue::UnitType::kTurns:
      return kCalcAngle;
    case CSSPrimitiveValue::UnitType::kMilliseconds:
    case CSSPrimitiveValue::UnitType::kSeconds:
      return kCalcTime;
    case CSSPrimitiveValue::UnitType::kHertz:
    case CSSPrimitiveValue::UnitType::kKilohertz:
      return kCalcFrequency;
    default:
      return kCalcOther;
  }
}

static bool HasDoubleValue(CSSPrimitiveValue::UnitType type) {
  switch (type) {
    case CSSPrimitiveValue::UnitType::kNumber:
    case CSSPrimitiveValue::UnitType::kPercentage:
    case CSSPrimitiveValue::UnitType::kEms:
    case CSSPrimitiveValue::UnitType::kExs:
    case CSSPrimitiveValue::UnitType::kChs:
    case CSSPrimitiveValue::UnitType::kRems:
    case CSSPrimitiveValue::UnitType::kPixels:
    case CSSPrimitiveValue::UnitType::kCentimeters:
    case CSSPrimitiveValue::UnitType::kMillimeters:
    case CSSPrimitiveValue::UnitType::kQuarterMillimeters:
    case CSSPrimitiveValue::UnitType::kInches:
    case CSSPrimitiveValue::UnitType::kPoints:
    case CSSPrimitiveValue::UnitType::kPicas:
    case CSSPrimitiveValue::UnitType::kUserUnits:
    case CSSPrimitiveValue::UnitType::kDegrees:
    case CSSPrimitiveValue::UnitType::kRadians:
    case CSSPrimitiveValue::UnitType::kGradians:
    case CSSPrimitiveValue::UnitType::kTurns:
    case CSSPrimitiveValue::UnitType::kMilliseconds:
    case CSSPrimitiveValue::UnitType::kSeconds:
    case CSSPrimitiveValue::UnitType::kHertz:
    case CSSPrimitiveValue::UnitType::kKilohertz:
    case CSSPrimitiveValue::UnitType::kViewportWidth:
    case CSSPrimitiveValue::UnitType::kViewportHeight:
    case CSSPrimitiveValue::UnitType::kViewportMin:
    case CSSPrimitiveValue::UnitType::kViewportMax:
    case CSSPrimitiveValue::UnitType::kDotsPerPixel:
    case CSSPrimitiveValue::UnitType::kDotsPerInch:
    case CSSPrimitiveValue::UnitType::kDotsPerCentimeter:
    case CSSPrimitiveValue::UnitType::kFraction:
    case CSSPrimitiveValue::UnitType::kInteger:
      return true;
    case CSSPrimitiveValue::UnitType::kUnknown:
    case CSSPrimitiveValue::UnitType::kCalc:
    case CSSPrimitiveValue::UnitType::kCalcPercentageWithNumber:
    case CSSPrimitiveValue::UnitType::kCalcPercentageWithLength:
    case CSSPrimitiveValue::UnitType::kCalcLengthWithNumber:
    case CSSPrimitiveValue::UnitType::kCalcPercentageWithLengthAndNumber:
    case CSSPrimitiveValue::UnitType::kQuirkyEms:
      return false;
  };
  NOTREACHED();
  return false;
}

static String BuildCSSText(const String& expression) {
  StringBuilder result;
  result.Append("calc");
  bool expression_has_single_term = expression[0] != '(';
  if (expression_has_single_term)
    result.Append('(');
  result.Append(expression);
  if (expression_has_single_term)
    result.Append(')');
  return result.ToString();
}

String CSSCalcValue::CustomCSSText() const {
  return BuildCSSText(expression_->CustomCSSText());
}

bool CSSCalcValue::Equals(const CSSCalcValue& other) const {
  return DataEquivalent(expression_, other.expression_);
}

double CSSCalcValue::ClampToPermittedRange(double value) const {
  return non_negative_ && value < 0 ? 0 : value;
}

double CSSCalcValue::DoubleValue() const {
  return ClampToPermittedRange(expression_->DoubleValue());
}

double CSSCalcValue::ComputeLengthPx(
    const CSSToLengthConversionData& conversion_data) const {
  return ClampToPermittedRange(expression_->ComputeLengthPx(conversion_data));
}

// ------ Start of CSSCalcPrimitiveValue member functions ------

// static
CSSCalcPrimitiveValue* CSSCalcPrimitiveValue::Create(CSSPrimitiveValue* value,
                                                     bool is_integer) {
  return MakeGarbageCollected<CSSCalcPrimitiveValue>(value, is_integer);
}

// static
CSSCalcPrimitiveValue* CSSCalcPrimitiveValue::Create(
    double value,
    CSSPrimitiveValue::UnitType type,
    bool is_integer) {
  if (std::isnan(value) || std::isinf(value))
    return nullptr;
  return MakeGarbageCollected<CSSCalcPrimitiveValue>(
      CSSNumericLiteralValue::Create(value, type), is_integer);
}

CSSCalcPrimitiveValue::CSSCalcPrimitiveValue(CSSPrimitiveValue* value,
                                             bool is_integer)
    : CSSCalcExpressionNode(UnitCategory(value->TypeWithCalcResolved()),
                            is_integer),
      value_(value) {}

bool CSSCalcPrimitiveValue::IsZero() const {
  return !value_->GetDoubleValue();
}

String CSSCalcPrimitiveValue::CustomCSSText() const {
  return value_->CssText();
}

void CSSCalcPrimitiveValue::AccumulatePixelsAndPercent(
    const CSSToLengthConversionData& conversion_data,
    PixelsAndPercent& value,
    float multiplier) const {
  switch (category_) {
    case kCalcLength:
      value.pixels = clampTo<float>(
          value.pixels +
          value_->ComputeLength<double>(conversion_data) * multiplier);
      break;
    case kCalcPercent:
      DCHECK(value_->IsPercentage());
      value.percent =
          clampTo<float>(value.percent + value_->GetDoubleValue() * multiplier);
      break;
    case kCalcNumber:
      // TODO(alancutter): Stop treating numbers like pixels unconditionally
      // in calcs to be able to accomodate border-image-width
      // https://drafts.csswg.org/css-backgrounds-3/#the-border-image-width
      value.pixels = clampTo<float>(value.pixels + value_->GetDoubleValue() *
                                                       conversion_data.Zoom() *
                                                       multiplier);
      break;
    default:
      NOTREACHED();
  }
}

double CSSCalcPrimitiveValue::DoubleValue() const {
  if (HasDoubleValue(TypeWithCalcResolved()))
    return value_->GetDoubleValue();
  NOTREACHED();
  return 0;
}

double CSSCalcPrimitiveValue::ComputeLengthPx(
    const CSSToLengthConversionData& conversion_data) const {
  switch (category_) {
    case kCalcLength:
      return value_->ComputeLength<double>(conversion_data);
    case kCalcNumber:
    case kCalcPercent:
      return value_->GetDoubleValue();
    case kCalcAngle:
    case kCalcFrequency:
    case kCalcPercentLength:
    case kCalcPercentNumber:
    case kCalcTime:
    case kCalcLengthNumber:
    case kCalcPercentLengthNumber:
    case kCalcOther:
      NOTREACHED();
      break;
  }
  NOTREACHED();
  return 0;
}

void CSSCalcPrimitiveValue::AccumulateLengthArray(CSSLengthArray& length_array,
                                                  double multiplier) const {
  DCHECK_NE(Category(), kCalcNumber);
  value_->AccumulateLengthArray(length_array, multiplier);
}

bool CSSCalcPrimitiveValue::operator==(
    const CSSCalcExpressionNode& other) const {
  if (!other.IsPrimitiveValue())
    return false;

  return DataEquivalent(value_, To<CSSCalcPrimitiveValue>(other).value_);
}

CSSPrimitiveValue::UnitType CSSCalcPrimitiveValue::TypeWithCalcResolved()
    const {
  return value_->TypeWithCalcResolved();
}

void CSSCalcPrimitiveValue::Trace(blink::Visitor* visitor) {
  visitor->Trace(value_);
  CSSCalcExpressionNode::Trace(visitor);
}

// ------ End of CSSCalcPrimitiveValue member functions

static const CalculationCategory kAddSubtractResult[kCalcOther][kCalcOther] = {
    /* CalcNumber */ {kCalcNumber, kCalcLengthNumber, kCalcPercentNumber,
                      kCalcPercentNumber, kCalcOther, kCalcOther, kCalcOther,
                      kCalcOther, kCalcLengthNumber, kCalcPercentLengthNumber},
    /* CalcLength */
    {kCalcLengthNumber, kCalcLength, kCalcPercentLength, kCalcOther,
     kCalcPercentLength, kCalcOther, kCalcOther, kCalcOther, kCalcLengthNumber,
     kCalcPercentLengthNumber},
    /* CalcPercent */
    {kCalcPercentNumber, kCalcPercentLength, kCalcPercent, kCalcPercentNumber,
     kCalcPercentLength, kCalcOther, kCalcOther, kCalcOther,
     kCalcPercentLengthNumber, kCalcPercentLengthNumber},
    /* CalcPercentNumber */
    {kCalcPercentNumber, kCalcPercentLengthNumber, kCalcPercentNumber,
     kCalcPercentNumber, kCalcPercentLengthNumber, kCalcOther, kCalcOther,
     kCalcOther, kCalcOther, kCalcPercentLengthNumber},
    /* CalcPercentLength */
    {kCalcPercentLengthNumber, kCalcPercentLength, kCalcPercentLength,
     kCalcPercentLengthNumber, kCalcPercentLength, kCalcOther, kCalcOther,
     kCalcOther, kCalcOther, kCalcPercentLengthNumber},
    /* CalcAngle  */
    {kCalcOther, kCalcOther, kCalcOther, kCalcOther, kCalcOther, kCalcAngle,
     kCalcOther, kCalcOther, kCalcOther, kCalcOther},
    /* CalcTime */
    {kCalcOther, kCalcOther, kCalcOther, kCalcOther, kCalcOther, kCalcOther,
     kCalcTime, kCalcOther, kCalcOther, kCalcOther},
    /* CalcFrequency */
    {kCalcOther, kCalcOther, kCalcOther, kCalcOther, kCalcOther, kCalcOther,
     kCalcOther, kCalcFrequency, kCalcOther, kCalcOther},
    /* CalcLengthNumber */
    {kCalcLengthNumber, kCalcLengthNumber, kCalcPercentLengthNumber,
     kCalcPercentLengthNumber, kCalcPercentLengthNumber, kCalcOther, kCalcOther,
     kCalcOther, kCalcLengthNumber, kCalcPercentLengthNumber},
    /* CalcPercentLengthNumber */
    {kCalcPercentLengthNumber, kCalcPercentLengthNumber,
     kCalcPercentLengthNumber, kCalcPercentLengthNumber,
     kCalcPercentLengthNumber, kCalcOther, kCalcOther, kCalcOther,
     kCalcPercentLengthNumber, kCalcPercentLengthNumber}};

static CalculationCategory DetermineCategory(
    const CSSCalcExpressionNode& left_side,
    const CSSCalcExpressionNode& right_side,
    CSSMathOperator op) {
  CalculationCategory left_category = left_side.Category();
  CalculationCategory right_category = right_side.Category();

  if (left_category == kCalcOther || right_category == kCalcOther)
    return kCalcOther;

  switch (op) {
    case CSSMathOperator::kAdd:
    case CSSMathOperator::kSubtract:
      return kAddSubtractResult[left_category][right_category];
    case CSSMathOperator::kMultiply:
      if (left_category != kCalcNumber && right_category != kCalcNumber)
        return kCalcOther;
      return left_category == kCalcNumber ? right_category : left_category;
    case CSSMathOperator::kDivide:
      if (right_category != kCalcNumber || right_side.IsZero())
        return kCalcOther;
      return left_category;
    default:
      break;
  }

  NOTREACHED();
  return kCalcOther;
}

static bool IsIntegerResult(const CSSCalcExpressionNode* left_side,
                            const CSSCalcExpressionNode* right_side,
                            CSSMathOperator op) {
  // Not testing for actual integer values.
  // Performs W3C spec's type checking for calc integers.
  // http://www.w3.org/TR/css3-values/#calc-type-checking
  return op != CSSMathOperator::kDivide && left_side->IsInteger() &&
         right_side->IsInteger();
}

// ------ Start of CSSCalcBinaryOperation member functions ------

// static
CSSCalcExpressionNode* CSSCalcBinaryOperation::Create(
    CSSCalcExpressionNode* left_side,
    CSSCalcExpressionNode* right_side,
    CSSMathOperator op) {
  DCHECK_NE(left_side->Category(), kCalcOther);
  DCHECK_NE(right_side->Category(), kCalcOther);

  CalculationCategory new_category =
      DetermineCategory(*left_side, *right_side, op);
  if (new_category == kCalcOther)
    return nullptr;

  return MakeGarbageCollected<CSSCalcBinaryOperation>(left_side, right_side, op,
                                                      new_category);
}

// static
CSSCalcExpressionNode* CSSCalcBinaryOperation::CreateSimplified(
    CSSCalcExpressionNode* left_side,
    CSSCalcExpressionNode* right_side,
    CSSMathOperator op) {
  CalculationCategory left_category = left_side->Category();
  CalculationCategory right_category = right_side->Category();
  DCHECK_NE(left_category, kCalcOther);
  DCHECK_NE(right_category, kCalcOther);

  bool is_integer = IsIntegerResult(left_side, right_side, op);

  // Simplify numbers.
  if (left_category == kCalcNumber && right_category == kCalcNumber) {
    return CSSCalcPrimitiveValue::Create(
        EvaluateOperator(left_side->DoubleValue(), right_side->DoubleValue(),
                         op),
        CSSPrimitiveValue::UnitType::kNumber, is_integer);
  }

  // Simplify addition and subtraction between same types.
  if (op == CSSMathOperator::kAdd || op == CSSMathOperator::kSubtract) {
    if (left_category == right_side->Category()) {
      CSSPrimitiveValue::UnitType left_type = left_side->TypeWithCalcResolved();
      if (HasDoubleValue(left_type)) {
        CSSPrimitiveValue::UnitType right_type =
            right_side->TypeWithCalcResolved();
        if (left_type == right_type) {
          return CSSCalcPrimitiveValue::Create(
              EvaluateOperator(left_side->DoubleValue(),
                               right_side->DoubleValue(), op),
              left_type, is_integer);
        }
        CSSPrimitiveValue::UnitCategory left_unit_category =
            CSSPrimitiveValue::UnitTypeToUnitCategory(left_type);
        if (left_unit_category != CSSPrimitiveValue::kUOther &&
            left_unit_category ==
                CSSPrimitiveValue::UnitTypeToUnitCategory(right_type)) {
          CSSPrimitiveValue::UnitType canonical_type =
              CSSPrimitiveValue::CanonicalUnitTypeForCategory(
                  left_unit_category);
          if (canonical_type != CSSPrimitiveValue::UnitType::kUnknown) {
            double left_value = clampTo<double>(
                left_side->DoubleValue() *
                CSSPrimitiveValue::ConversionToCanonicalUnitsScaleFactor(
                    left_type));
            double right_value = clampTo<double>(
                right_side->DoubleValue() *
                CSSPrimitiveValue::ConversionToCanonicalUnitsScaleFactor(
                    right_type));
            return CSSCalcPrimitiveValue::Create(
                EvaluateOperator(left_value, right_value, op), canonical_type,
                is_integer);
          }
        }
      }
    }
  } else {
    // Simplify multiplying or dividing by a number for simplifiable types.
    DCHECK(op == CSSMathOperator::kMultiply || op == CSSMathOperator::kDivide);
    CSSCalcExpressionNode* number_side = GetNumberSide(left_side, right_side);
    if (!number_side)
      return Create(left_side, right_side, op);
    if (number_side == left_side && op == CSSMathOperator::kDivide)
      return nullptr;
    CSSCalcExpressionNode* other_side =
        left_side == number_side ? right_side : left_side;

    double number = number_side->DoubleValue();
    if (std::isnan(number) || std::isinf(number))
      return nullptr;
    if (op == CSSMathOperator::kDivide && !number)
      return nullptr;

    CSSPrimitiveValue::UnitType other_type = other_side->TypeWithCalcResolved();
    if (HasDoubleValue(other_type)) {
      return CSSCalcPrimitiveValue::Create(
          EvaluateOperator(other_side->DoubleValue(), number, op), other_type,
          is_integer);
    }
  }

  return Create(left_side, right_side, op);
}

CSSCalcBinaryOperation::CSSCalcBinaryOperation(
    CSSCalcExpressionNode* left_side,
    CSSCalcExpressionNode* right_side,
    CSSMathOperator op,
    CalculationCategory category)
    : CSSCalcExpressionNode(category,
                            IsIntegerResult(left_side, right_side, op)),
      left_side_(left_side),
      right_side_(right_side),
      operator_(op) {}

bool CSSCalcBinaryOperation::IsZero() const {
  return !DoubleValue();
}

void CSSCalcBinaryOperation::AccumulatePixelsAndPercent(
    const CSSToLengthConversionData& conversion_data,
    PixelsAndPercent& value,
    float multiplier) const {
  switch (operator_) {
    case CSSMathOperator::kAdd:
      left_side_->AccumulatePixelsAndPercent(conversion_data, value,
                                             multiplier);
      right_side_->AccumulatePixelsAndPercent(conversion_data, value,
                                              multiplier);
      break;
    case CSSMathOperator::kSubtract:
      left_side_->AccumulatePixelsAndPercent(conversion_data, value,
                                             multiplier);
      right_side_->AccumulatePixelsAndPercent(conversion_data, value,
                                              -multiplier);
      break;
    case CSSMathOperator::kMultiply:
      DCHECK_NE((left_side_->Category() == kCalcNumber),
                (right_side_->Category() == kCalcNumber));
      if (left_side_->Category() == kCalcNumber) {
        right_side_->AccumulatePixelsAndPercent(
            conversion_data, value, multiplier * left_side_->DoubleValue());
      } else {
        left_side_->AccumulatePixelsAndPercent(
            conversion_data, value, multiplier * right_side_->DoubleValue());
      }
      break;
    case CSSMathOperator::kDivide:
      DCHECK_EQ(right_side_->Category(), kCalcNumber);
      left_side_->AccumulatePixelsAndPercent(
          conversion_data, value, multiplier / right_side_->DoubleValue());
      break;
    default:
      NOTREACHED();
  }
}

double CSSCalcBinaryOperation::DoubleValue() const {
  return Evaluate(left_side_->DoubleValue(), right_side_->DoubleValue());
}

double CSSCalcBinaryOperation::ComputeLengthPx(
    const CSSToLengthConversionData& conversion_data) const {
  const double left_value = left_side_->ComputeLengthPx(conversion_data);
  const double right_value = right_side_->ComputeLengthPx(conversion_data);
  return Evaluate(left_value, right_value);
}

void CSSCalcBinaryOperation::AccumulateLengthArray(CSSLengthArray& length_array,
                                                   double multiplier) const {
  switch (operator_) {
    case CSSMathOperator::kAdd:
      left_side_->AccumulateLengthArray(length_array, multiplier);
      right_side_->AccumulateLengthArray(length_array, multiplier);
      break;
    case CSSMathOperator::kSubtract:
      left_side_->AccumulateLengthArray(length_array, multiplier);
      right_side_->AccumulateLengthArray(length_array, -multiplier);
      break;
    case CSSMathOperator::kMultiply:
      DCHECK_NE((left_side_->Category() == kCalcNumber),
                (right_side_->Category() == kCalcNumber));
      if (left_side_->Category() == kCalcNumber) {
        right_side_->AccumulateLengthArray(
            length_array, multiplier * left_side_->DoubleValue());
      } else {
        left_side_->AccumulateLengthArray(
            length_array, multiplier * right_side_->DoubleValue());
      }
      break;
    case CSSMathOperator::kDivide:
      DCHECK_EQ(right_side_->Category(), kCalcNumber);
      left_side_->AccumulateLengthArray(
          length_array, multiplier / right_side_->DoubleValue());
      break;
    default:
      NOTREACHED();
  }
}

// static
String CSSCalcBinaryOperation::BuildCSSText(const String& left_expression,
                                            const String& right_expression,
                                            CSSMathOperator op) {
  StringBuilder result;
  result.Append('(');
  result.Append(left_expression);
  result.Append(' ');
  result.Append(ToString(op));
  result.Append(' ');
  result.Append(right_expression);
  result.Append(')');

  return result.ToString();
}

String CSSCalcBinaryOperation::CustomCSSText() const {
  return BuildCSSText(left_side_->CustomCSSText(), right_side_->CustomCSSText(),
                      operator_);
}

bool CSSCalcBinaryOperation::operator==(
    const CSSCalcExpressionNode& exp) const {
  if (!exp.IsBinaryOperation())
    return false;

  const CSSCalcBinaryOperation& other = To<CSSCalcBinaryOperation>(exp);
  return DataEquivalent(left_side_, other.left_side_) &&
         DataEquivalent(right_side_, other.right_side_) &&
         operator_ == other.operator_;
}

CSSPrimitiveValue::UnitType CSSCalcBinaryOperation::TypeWithCalcResolved()
    const {
  switch (category_) {
    case kCalcNumber:
      DCHECK_EQ(left_side_->Category(), kCalcNumber);
      DCHECK_EQ(right_side_->Category(), kCalcNumber);
      return CSSPrimitiveValue::UnitType::kNumber;
    case kCalcLength:
    case kCalcPercent: {
      if (left_side_->Category() == kCalcNumber)
        return right_side_->TypeWithCalcResolved();
      if (right_side_->Category() == kCalcNumber)
        return left_side_->TypeWithCalcResolved();
      CSSPrimitiveValue::UnitType left_type =
          left_side_->TypeWithCalcResolved();
      if (left_type == right_side_->TypeWithCalcResolved())
        return left_type;
      return CSSPrimitiveValue::UnitType::kUnknown;
    }
    case kCalcAngle:
      return CSSPrimitiveValue::UnitType::kDegrees;
    case kCalcTime:
      return CSSPrimitiveValue::UnitType::kMilliseconds;
    case kCalcFrequency:
      return CSSPrimitiveValue::UnitType::kHertz;
    case kCalcPercentLength:
    case kCalcPercentNumber:
    case kCalcLengthNumber:
    case kCalcPercentLengthNumber:
    case kCalcOther:
      return CSSPrimitiveValue::UnitType::kUnknown;
  }
  NOTREACHED();
  return CSSPrimitiveValue::UnitType::kUnknown;
}

void CSSCalcBinaryOperation::Trace(blink::Visitor* visitor) {
  visitor->Trace(left_side_);
  visitor->Trace(right_side_);
  CSSCalcExpressionNode::Trace(visitor);
}

// static
CSSCalcExpressionNode* CSSCalcBinaryOperation::GetNumberSide(
    CSSCalcExpressionNode* left_side,
    CSSCalcExpressionNode* right_side) {
  if (left_side->Category() == kCalcNumber)
    return left_side;
  if (right_side->Category() == kCalcNumber)
    return right_side;
  return nullptr;
}

// static
double CSSCalcBinaryOperation::EvaluateOperator(double left_value,
                                                double right_value,
                                                CSSMathOperator op) {
  switch (op) {
    case CSSMathOperator::kAdd:
      return clampTo<double>(left_value + right_value);
    case CSSMathOperator::kSubtract:
      return clampTo<double>(left_value - right_value);
    case CSSMathOperator::kMultiply:
      return clampTo<double>(left_value * right_value);
    case CSSMathOperator::kDivide:
      if (right_value)
        return clampTo<double>(left_value / right_value);
      return std::numeric_limits<double>::quiet_NaN();
    default:
      NOTREACHED();
      break;
  }
  return 0;
}

// ------ End of CSSCalcBinaryOperation member functions ------

static ParseState CheckDepthAndIndex(int* depth, CSSParserTokenRange tokens) {
  (*depth)++;
  if (tokens.AtEnd())
    return NoMoreTokens;
  if (*depth > maxExpressionDepth)
    return TooDeep;
  return OK;
}

class CSSCalcExpressionNodeParser {
  STACK_ALLOCATED();

 public:
  CSSCalcExpressionNodeParser() {}

  CSSCalcExpressionNode* ParseCalc(CSSParserTokenRange tokens) {
    tokens.ConsumeWhitespace();
    CSSCalcExpressionNode* result = ParseValueExpression(tokens, 0);
    if (!result || !tokens.AtEnd())
      return nullptr;
    return result;
  }

 private:
  CSSCalcExpressionNode* ParseValue(CSSParserTokenRange& tokens) {
    CSSParserToken token = tokens.ConsumeIncludingWhitespace();
    if (!(token.GetType() == kNumberToken ||
          token.GetType() == kPercentageToken ||
          token.GetType() == kDimensionToken))
      return nullptr;

    CSSPrimitiveValue::UnitType type = token.GetUnitType();
    if (UnitCategory(type) == kCalcOther)
      return nullptr;

    return CSSCalcPrimitiveValue::Create(
        CSSNumericLiteralValue::Create(token.NumericValue(), type),
        token.GetNumericValueType() == kIntegerValueType);
  }

  CSSCalcExpressionNode* ParseValueTerm(CSSParserTokenRange& tokens,
                                        int depth) {
    if (CheckDepthAndIndex(&depth, tokens) != OK)
      return nullptr;

    if (tokens.Peek().GetType() == kLeftParenthesisToken ||
        tokens.Peek().FunctionId() == CSSValueID::kCalc) {
      CSSParserTokenRange inner_range = tokens.ConsumeBlock();
      tokens.ConsumeWhitespace();
      inner_range.ConsumeWhitespace();
      CSSCalcExpressionNode* result = ParseValueExpression(inner_range, depth);
      if (!result)
        return nullptr;
      result->SetIsNestedCalc();
      return result;
    }

    return ParseValue(tokens);
  }

  CSSCalcExpressionNode* ParseValueMultiplicativeExpression(
      CSSParserTokenRange& tokens,
      int depth) {
    if (CheckDepthAndIndex(&depth, tokens) != OK)
      return nullptr;

    CSSCalcExpressionNode* result = ParseValueTerm(tokens, depth);
    if (!result)
      return nullptr;

    while (!tokens.AtEnd()) {
      CSSMathOperator math_operator = ParseCSSArithmeticOperator(tokens.Peek());
      if (math_operator != CSSMathOperator::kMultiply &&
          math_operator != CSSMathOperator::kDivide)
        break;
      tokens.ConsumeIncludingWhitespace();

      CSSCalcExpressionNode* rhs = ParseValueTerm(tokens, depth);
      if (!rhs)
        return nullptr;

      result =
          CSSCalcBinaryOperation::CreateSimplified(result, rhs, math_operator);

      if (!result)
        return nullptr;
    }

    return result;
  }

  CSSCalcExpressionNode* ParseAdditiveValueExpression(
      CSSParserTokenRange& tokens,
      int depth) {
    if (CheckDepthAndIndex(&depth, tokens) != OK)
      return nullptr;

    CSSCalcExpressionNode* result =
        ParseValueMultiplicativeExpression(tokens, depth);
    if (!result)
      return nullptr;

    while (!tokens.AtEnd()) {
      CSSMathOperator math_operator = ParseCSSArithmeticOperator(tokens.Peek());
      if (math_operator != CSSMathOperator::kAdd &&
          math_operator != CSSMathOperator::kSubtract)
        break;
      if ((&tokens.Peek() - 1)->GetType() != kWhitespaceToken)
        return nullptr;  // calc(1px+ 2px) is invalid
      tokens.Consume();
      if (tokens.Peek().GetType() != kWhitespaceToken)
        return nullptr;  // calc(1px +2px) is invalid
      tokens.ConsumeIncludingWhitespace();

      CSSCalcExpressionNode* rhs =
          ParseValueMultiplicativeExpression(tokens, depth);
      if (!rhs)
        return nullptr;

      result =
          CSSCalcBinaryOperation::CreateSimplified(result, rhs, math_operator);

      if (!result)
        return nullptr;
    }

    return result;
  }

  CSSCalcExpressionNode* ParseValueExpression(CSSParserTokenRange& tokens,
                                              int depth) {
    return ParseAdditiveValueExpression(tokens, depth);
  }
};

CSSCalcExpressionNode* CSSCalcValue::CreateExpressionNode(
    CSSPrimitiveValue* value,
    bool is_integer) {
  return CSSCalcPrimitiveValue::Create(value, is_integer);
}

CSSCalcExpressionNode* CSSCalcValue::CreateExpressionNode(
    CSSCalcExpressionNode* left_side,
    CSSCalcExpressionNode* right_side,
    CSSMathOperator op) {
  return CSSCalcBinaryOperation::Create(left_side, right_side, op);
}

CSSCalcExpressionNode* CSSCalcValue::CreateExpressionNode(double pixels,
                                                          double percent) {
  return CreateExpressionNode(
      CreateExpressionNode(
          CSSNumericLiteralValue::Create(
              percent, CSSPrimitiveValue::UnitType::kPercentage),
          percent == trunc(percent)),
      CreateExpressionNode(CSSNumericLiteralValue::Create(
                               pixels, CSSPrimitiveValue::UnitType::kPixels),
                           pixels == trunc(pixels)),
      CSSMathOperator::kAdd);
}

CSSCalcValue* CSSCalcValue::Create(const CSSParserTokenRange& tokens,
                                   ValueRange range) {
  CSSCalcExpressionNodeParser parser;
  CSSCalcExpressionNode* expression = parser.ParseCalc(tokens);

  return expression ? MakeGarbageCollected<CSSCalcValue>(expression, range)
                    : nullptr;
}

CSSCalcValue* CSSCalcValue::Create(CSSCalcExpressionNode* expression,
                                   ValueRange range) {
  return MakeGarbageCollected<CSSCalcValue>(expression, range);
}

}  // namespace blink
