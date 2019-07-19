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

#include "third_party/blink/renderer/core/css/css_math_expression_node.h"

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
    default:
      return false;
  }
}

// ------ Start of CSSMathExpressionNumericLiteral member functions ------

// static
CSSMathExpressionNumericLiteral* CSSMathExpressionNumericLiteral::Create(
    CSSNumericLiteralValue* value,
    bool is_integer) {
  return MakeGarbageCollected<CSSMathExpressionNumericLiteral>(value,
                                                               is_integer);
}

// static
CSSMathExpressionNumericLiteral* CSSMathExpressionNumericLiteral::Create(
    double value,
    CSSPrimitiveValue::UnitType type,
    bool is_integer) {
  if (std::isnan(value) || std::isinf(value))
    return nullptr;
  return MakeGarbageCollected<CSSMathExpressionNumericLiteral>(
      CSSNumericLiteralValue::Create(value, type), is_integer);
}

CSSMathExpressionNumericLiteral::CSSMathExpressionNumericLiteral(
    CSSNumericLiteralValue* value,
    bool is_integer)
    : CSSMathExpressionNode(UnitCategory(value->GetType()), is_integer),
      value_(value) {}

bool CSSMathExpressionNumericLiteral::IsZero() const {
  return !value_->GetDoubleValue();
}

String CSSMathExpressionNumericLiteral::CustomCSSText() const {
  return value_->CssText();
}

void CSSMathExpressionNumericLiteral::AccumulatePixelsAndPercent(
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

double CSSMathExpressionNumericLiteral::DoubleValue() const {
  if (HasDoubleValue(ResolvedUnitType()))
    return value_->GetDoubleValue();
  NOTREACHED();
  return 0;
}

base::Optional<double>
CSSMathExpressionNumericLiteral::ComputeValueInCanonicalUnit() const {
  switch (category_) {
    case kCalcNumber:
    case kCalcPercent:
      return value_->DoubleValue();
    case kCalcLength:
      if (CSSPrimitiveValue::IsRelativeUnit(value_->GetType()))
        return base::nullopt;
      U_FALLTHROUGH;
    case kCalcAngle:
    case kCalcTime:
    case kCalcFrequency:
      return value_->DoubleValue() *
             CSSPrimitiveValue::ConversionToCanonicalUnitsScaleFactor(
                 value_->GetType());
    default:
      return base::nullopt;
  }
}

double CSSMathExpressionNumericLiteral::ComputeLengthPx(
    const CSSToLengthConversionData& conversion_data) const {
  switch (category_) {
    case kCalcLength:
      return value_->ComputeLength<double>(conversion_data);
    case kCalcNumber:
    case kCalcPercent:
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

void CSSMathExpressionNumericLiteral::AccumulateLengthArray(
    CSSLengthArray& length_array,
    double multiplier) const {
  DCHECK_NE(Category(), kCalcNumber);
  value_->AccumulateLengthArray(length_array, multiplier);
}

bool CSSMathExpressionNumericLiteral::operator==(
    const CSSMathExpressionNode& other) const {
  if (!other.IsNumericLiteral())
    return false;

  return DataEquivalent(value_,
                        To<CSSMathExpressionNumericLiteral>(other).value_);
}

CSSPrimitiveValue::UnitType CSSMathExpressionNumericLiteral::ResolvedUnitType()
    const {
  return value_->GetType();
}

bool CSSMathExpressionNumericLiteral::IsComputationallyIndependent() const {
  return value_->IsComputationallyIndependent();
}

void CSSMathExpressionNumericLiteral::Trace(blink::Visitor* visitor) {
  visitor->Trace(value_);
  CSSMathExpressionNode::Trace(visitor);
}

// ------ End of CSSMathExpressionNumericLiteral member functions

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
    const CSSMathExpressionNode& left_side,
    const CSSMathExpressionNode& right_side,
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

static bool IsIntegerResult(const CSSMathExpressionNode* left_side,
                            const CSSMathExpressionNode* right_side,
                            CSSMathOperator op) {
  // Not testing for actual integer values.
  // Performs W3C spec's type checking for calc integers.
  // http://www.w3.org/TR/css3-values/#calc-type-checking
  return op != CSSMathOperator::kDivide && left_side->IsInteger() &&
         right_side->IsInteger();
}

// ------ Start of CSSMathExpressionBinaryOperation member functions ------

// static
CSSMathExpressionNode* CSSMathExpressionBinaryOperation::Create(
    CSSMathExpressionNode* left_side,
    CSSMathExpressionNode* right_side,
    CSSMathOperator op) {
  DCHECK_NE(left_side->Category(), kCalcOther);
  DCHECK_NE(right_side->Category(), kCalcOther);

  CalculationCategory new_category =
      DetermineCategory(*left_side, *right_side, op);
  if (new_category == kCalcOther)
    return nullptr;

  return MakeGarbageCollected<CSSMathExpressionBinaryOperation>(
      left_side, right_side, op, new_category);
}

// static
CSSMathExpressionNode* CSSMathExpressionBinaryOperation::CreateSimplified(
    CSSMathExpressionNode* left_side,
    CSSMathExpressionNode* right_side,
    CSSMathOperator op) {
  CalculationCategory left_category = left_side->Category();
  CalculationCategory right_category = right_side->Category();
  DCHECK_NE(left_category, kCalcOther);
  DCHECK_NE(right_category, kCalcOther);

  bool is_integer = IsIntegerResult(left_side, right_side, op);

  // Simplify numbers.
  if (left_category == kCalcNumber && right_category == kCalcNumber) {
    return CSSMathExpressionNumericLiteral::Create(
        EvaluateOperator(left_side->DoubleValue(), right_side->DoubleValue(),
                         op),
        CSSPrimitiveValue::UnitType::kNumber, is_integer);
  }

  // Simplify addition and subtraction between same types.
  if (op == CSSMathOperator::kAdd || op == CSSMathOperator::kSubtract) {
    if (left_category == right_side->Category()) {
      CSSPrimitiveValue::UnitType left_type = left_side->ResolvedUnitType();
      if (HasDoubleValue(left_type)) {
        CSSPrimitiveValue::UnitType right_type = right_side->ResolvedUnitType();
        if (left_type == right_type) {
          return CSSMathExpressionNumericLiteral::Create(
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
            return CSSMathExpressionNumericLiteral::Create(
                EvaluateOperator(left_value, right_value, op), canonical_type,
                is_integer);
          }
        }
      }
    }
  } else {
    // Simplify multiplying or dividing by a number for simplifiable types.
    DCHECK(op == CSSMathOperator::kMultiply || op == CSSMathOperator::kDivide);
    CSSMathExpressionNode* number_side = GetNumberSide(left_side, right_side);
    if (!number_side)
      return Create(left_side, right_side, op);
    if (number_side == left_side && op == CSSMathOperator::kDivide)
      return nullptr;
    CSSMathExpressionNode* other_side =
        left_side == number_side ? right_side : left_side;

    double number = number_side->DoubleValue();
    if (std::isnan(number) || std::isinf(number))
      return nullptr;
    if (op == CSSMathOperator::kDivide && !number)
      return nullptr;

    CSSPrimitiveValue::UnitType other_type = other_side->ResolvedUnitType();
    if (HasDoubleValue(other_type)) {
      return CSSMathExpressionNumericLiteral::Create(
          EvaluateOperator(other_side->DoubleValue(), number, op), other_type,
          is_integer);
    }
  }

  return Create(left_side, right_side, op);
}

CSSMathExpressionBinaryOperation::CSSMathExpressionBinaryOperation(
    CSSMathExpressionNode* left_side,
    CSSMathExpressionNode* right_side,
    CSSMathOperator op,
    CalculationCategory category)
    : CSSMathExpressionNode(category,
                            IsIntegerResult(left_side, right_side, op)),
      left_side_(left_side),
      right_side_(right_side),
      operator_(op) {}

bool CSSMathExpressionBinaryOperation::IsZero() const {
  return !DoubleValue();
}

void CSSMathExpressionBinaryOperation::AccumulatePixelsAndPercent(
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

double CSSMathExpressionBinaryOperation::DoubleValue() const {
  DCHECK(HasDoubleValue(ResolvedUnitType())) << CustomCSSText();
  return Evaluate(left_side_->DoubleValue(), right_side_->DoubleValue());
}

static bool HasCanonicalUnit(CalculationCategory category) {
  return category == kCalcNumber || category == kCalcLength ||
         category == kCalcPercent || category == kCalcAngle ||
         category == kCalcTime || category == kCalcFrequency;
}

base::Optional<double>
CSSMathExpressionBinaryOperation::ComputeValueInCanonicalUnit() const {
  if (!HasCanonicalUnit(category_))
    return base::nullopt;

  base::Optional<double> left_value = left_side_->ComputeValueInCanonicalUnit();
  if (!left_value)
    return base::nullopt;

  base::Optional<double> right_value =
      right_side_->ComputeValueInCanonicalUnit();
  if (!right_value)
    return base::nullopt;

  return Evaluate(*left_value, *right_value);
}

double CSSMathExpressionBinaryOperation::ComputeLengthPx(
    const CSSToLengthConversionData& conversion_data) const {
  DCHECK_EQ(kCalcLength, Category());
  double left_value;
  if (left_side_->Category() == kCalcLength) {
    left_value = left_side_->ComputeLengthPx(conversion_data);
  } else {
    DCHECK_EQ(kCalcNumber, left_side_->Category());
    left_value = left_side_->DoubleValue();
  }
  double right_value;
  if (right_side_->Category() == kCalcLength) {
    right_value = right_side_->ComputeLengthPx(conversion_data);
  } else {
    DCHECK_EQ(kCalcNumber, right_side_->Category());
    right_value = right_side_->DoubleValue();
  }
  return Evaluate(left_value, right_value);
}

void CSSMathExpressionBinaryOperation::AccumulateLengthArray(
    CSSLengthArray& length_array,
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

bool CSSMathExpressionBinaryOperation::IsComputationallyIndependent() const {
  if (Category() != kCalcLength && Category() != kCalcPercentLength)
    return true;
  return left_side_->IsComputationallyIndependent() &&
         right_side_->IsComputationallyIndependent();
}

// static
String CSSMathExpressionBinaryOperation::BuildCSSText(
    const String& left_expression,
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

String CSSMathExpressionBinaryOperation::CustomCSSText() const {
  return BuildCSSText(left_side_->CustomCSSText(), right_side_->CustomCSSText(),
                      operator_);
}

bool CSSMathExpressionBinaryOperation::operator==(
    const CSSMathExpressionNode& exp) const {
  if (!exp.IsBinaryOperation())
    return false;

  const CSSMathExpressionBinaryOperation& other =
      To<CSSMathExpressionBinaryOperation>(exp);
  return DataEquivalent(left_side_, other.left_side_) &&
         DataEquivalent(right_side_, other.right_side_) &&
         operator_ == other.operator_;
}

CSSPrimitiveValue::UnitType CSSMathExpressionBinaryOperation::ResolvedUnitType()
    const {
  switch (category_) {
    case kCalcNumber:
      DCHECK_EQ(left_side_->Category(), kCalcNumber);
      DCHECK_EQ(right_side_->Category(), kCalcNumber);
      return CSSPrimitiveValue::UnitType::kNumber;
    case kCalcLength:
    case kCalcPercent: {
      if (left_side_->Category() == kCalcNumber)
        return right_side_->ResolvedUnitType();
      if (right_side_->Category() == kCalcNumber)
        return left_side_->ResolvedUnitType();
      CSSPrimitiveValue::UnitType left_type = left_side_->ResolvedUnitType();
      if (left_type == right_side_->ResolvedUnitType())
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

void CSSMathExpressionBinaryOperation::Trace(blink::Visitor* visitor) {
  visitor->Trace(left_side_);
  visitor->Trace(right_side_);
  CSSMathExpressionNode::Trace(visitor);
}

// static
CSSMathExpressionNode* CSSMathExpressionBinaryOperation::GetNumberSide(
    CSSMathExpressionNode* left_side,
    CSSMathExpressionNode* right_side) {
  if (left_side->Category() == kCalcNumber)
    return left_side;
  if (right_side->Category() == kCalcNumber)
    return right_side;
  return nullptr;
}

// static
double CSSMathExpressionBinaryOperation::EvaluateOperator(double left_value,
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

// ------ End of CSSMathExpressionBinaryOperation member functions ------

static ParseState CheckDepthAndIndex(int* depth, CSSParserTokenRange tokens) {
  (*depth)++;
  if (tokens.AtEnd())
    return NoMoreTokens;
  if (*depth > maxExpressionDepth)
    return TooDeep;
  return OK;
}

class CSSMathExpressionNodeParser {
  STACK_ALLOCATED();

 public:
  CSSMathExpressionNodeParser() {}

  CSSMathExpressionNode* ParseCalc(CSSParserTokenRange tokens) {
    tokens.ConsumeWhitespace();
    CSSMathExpressionNode* result = ParseValueExpression(tokens, 0);
    if (!result || !tokens.AtEnd())
      return nullptr;
    return result;
  }

 private:
  CSSMathExpressionNode* ParseValue(CSSParserTokenRange& tokens) {
    CSSParserToken token = tokens.ConsumeIncludingWhitespace();
    if (!(token.GetType() == kNumberToken ||
          token.GetType() == kPercentageToken ||
          token.GetType() == kDimensionToken))
      return nullptr;

    CSSPrimitiveValue::UnitType type = token.GetUnitType();
    if (UnitCategory(type) == kCalcOther)
      return nullptr;

    return CSSMathExpressionNumericLiteral::Create(
        CSSNumericLiteralValue::Create(token.NumericValue(), type),
        token.GetNumericValueType() == kIntegerValueType);
  }

  CSSMathExpressionNode* ParseValueTerm(CSSParserTokenRange& tokens,
                                        int depth) {
    if (CheckDepthAndIndex(&depth, tokens) != OK)
      return nullptr;

    if (tokens.Peek().GetType() == kLeftParenthesisToken ||
        tokens.Peek().FunctionId() == CSSValueID::kCalc) {
      CSSParserTokenRange inner_range = tokens.ConsumeBlock();
      tokens.ConsumeWhitespace();
      inner_range.ConsumeWhitespace();
      CSSMathExpressionNode* result = ParseValueExpression(inner_range, depth);
      if (!result)
        return nullptr;
      result->SetIsNestedCalc();
      return result;
    }

    return ParseValue(tokens);
  }

  CSSMathExpressionNode* ParseValueMultiplicativeExpression(
      CSSParserTokenRange& tokens,
      int depth) {
    if (CheckDepthAndIndex(&depth, tokens) != OK)
      return nullptr;

    CSSMathExpressionNode* result = ParseValueTerm(tokens, depth);
    if (!result)
      return nullptr;

    while (!tokens.AtEnd()) {
      CSSMathOperator math_operator = ParseCSSArithmeticOperator(tokens.Peek());
      if (math_operator != CSSMathOperator::kMultiply &&
          math_operator != CSSMathOperator::kDivide)
        break;
      tokens.ConsumeIncludingWhitespace();

      CSSMathExpressionNode* rhs = ParseValueTerm(tokens, depth);
      if (!rhs)
        return nullptr;

      result = CSSMathExpressionBinaryOperation::CreateSimplified(
          result, rhs, math_operator);

      if (!result)
        return nullptr;
    }

    return result;
  }

  CSSMathExpressionNode* ParseAdditiveValueExpression(
      CSSParserTokenRange& tokens,
      int depth) {
    if (CheckDepthAndIndex(&depth, tokens) != OK)
      return nullptr;

    CSSMathExpressionNode* result =
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

      CSSMathExpressionNode* rhs =
          ParseValueMultiplicativeExpression(tokens, depth);
      if (!rhs)
        return nullptr;

      result = CSSMathExpressionBinaryOperation::CreateSimplified(
          result, rhs, math_operator);

      if (!result)
        return nullptr;
    }

    return result;
  }

  CSSMathExpressionNode* ParseValueExpression(CSSParserTokenRange& tokens,
                                              int depth) {
    return ParseAdditiveValueExpression(tokens, depth);
  }
};

// static
CSSMathExpressionNode* CSSMathExpressionNode::CreateFromPixelsAndPercent(
    double pixels,
    double percent) {
  return CSSMathExpressionBinaryOperation::Create(
      CSSMathExpressionNumericLiteral::Create(
          CSSNumericLiteralValue::Create(
              percent, CSSPrimitiveValue::UnitType::kPercentage),
          percent == trunc(percent)),
      CSSMathExpressionNumericLiteral::Create(
          CSSNumericLiteralValue::Create(pixels,
                                         CSSPrimitiveValue::UnitType::kPixels),
          pixels == trunc(pixels)),
      CSSMathOperator::kAdd);
}

// static
CSSMathExpressionNode* CSSMathExpressionNode::ParseCalc(
    const CSSParserTokenRange& tokens) {
  CSSMathExpressionNodeParser parser;
  return parser.ParseCalc(tokens);
}

}  // namespace blink
