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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CALCULATION_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CALCULATION_VALUE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_math_operator.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/platform/geometry/calculation_value.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class CalculationValue;

// The order of this enum should not change since its elements are used as
// indices in the addSubtractResult matrix.
enum CalculationCategory {
  kCalcNumber = 0,
  kCalcLength,
  kCalcPercent,
  kCalcPercentNumber,
  kCalcPercentLength,
  kCalcAngle,
  kCalcTime,
  kCalcFrequency,
  kCalcLengthNumber,
  kCalcPercentLengthNumber,
  kCalcOther
};

class CSSCalcExpressionNode : public GarbageCollected<CSSCalcExpressionNode> {
 public:
  virtual bool IsPrimitiveValue() const { return false; }
  virtual bool IsBinaryOperation() const { return false; }

  virtual bool IsZero() const = 0;
  virtual double DoubleValue() const = 0;
  virtual double ComputeLengthPx(const CSSToLengthConversionData&) const = 0;
  virtual void AccumulateLengthArray(CSSLengthArray&,
                                     double multiplier) const = 0;
  virtual void AccumulatePixelsAndPercent(const CSSToLengthConversionData&,
                                          PixelsAndPercent&,
                                          float multiplier = 1) const = 0;
  virtual String CustomCSSText() const = 0;
  virtual bool operator==(const CSSCalcExpressionNode& other) const {
    return category_ == other.category_ && is_integer_ == other.is_integer_;
  }

  CalculationCategory Category() const { return category_; }
  virtual CSSPrimitiveValue::UnitType TypeWithCalcResolved() const = 0;
  bool IsInteger() const { return is_integer_; }

  bool IsNestedCalc() const { return is_nested_calc_; }
  void SetIsNestedCalc() { is_nested_calc_ = true; }

  virtual void Trace(blink::Visitor* visitor) {}

 protected:
  CSSCalcExpressionNode(CalculationCategory category, bool is_integer)
      : category_(category), is_integer_(is_integer) {
    DCHECK_NE(category, kCalcOther);
  }

  CalculationCategory category_;
  bool is_integer_;
  bool is_nested_calc_ = false;
};

// TODO(crbug.com/825895): Rename it and make it store a CSSNumericLiteralValue
class CSSCalcPrimitiveValue final : public CSSCalcExpressionNode {
 public:
  static CSSCalcPrimitiveValue* Create(CSSPrimitiveValue* value,
                                       bool is_integer);
  static CSSCalcPrimitiveValue* Create(double value,
                                       CSSPrimitiveValue::UnitType type,
                                       bool is_integer);

  CSSCalcPrimitiveValue(CSSPrimitiveValue* value, bool is_integer);

  bool IsPrimitiveValue() const final { return true; }

  bool IsZero() const final;
  String CustomCSSText() const final;
  void AccumulatePixelsAndPercent(
      const CSSToLengthConversionData& conversion_data,
      PixelsAndPercent& value,
      float multiplier) const final;
  double DoubleValue() const final;
  double ComputeLengthPx(
      const CSSToLengthConversionData& conversion_data) const final;
  void AccumulateLengthArray(CSSLengthArray& length_array,
                             double multiplier) const final;
  bool operator==(const CSSCalcExpressionNode& other) const final;
  CSSPrimitiveValue::UnitType TypeWithCalcResolved() const final;
  void Trace(blink::Visitor* visitor) final;

 private:
  Member<CSSPrimitiveValue> value_;
};

template <>
struct DowncastTraits<CSSCalcPrimitiveValue> {
  static bool AllowFrom(const CSSCalcExpressionNode& node) {
    return node.IsPrimitiveValue();
  }
};

class CSSCalcBinaryOperation final : public CSSCalcExpressionNode {
 public:
  static CSSCalcExpressionNode* Create(CSSCalcExpressionNode* left_side,
                                       CSSCalcExpressionNode* right_side,
                                       CSSMathOperator op);
  static CSSCalcExpressionNode* CreateSimplified(
      CSSCalcExpressionNode* left_side,
      CSSCalcExpressionNode* right_side,
      CSSMathOperator op);

  CSSCalcBinaryOperation(CSSCalcExpressionNode* left_side,
                         CSSCalcExpressionNode* right_side,
                         CSSMathOperator op,
                         CalculationCategory category);

  const CSSCalcExpressionNode* LeftExpressionNode() const { return left_side_; }
  const CSSCalcExpressionNode* RightExpressionNode() const {
    return right_side_;
  }
  CSSMathOperator OperatorType() const { return operator_; }

  bool IsBinaryOperation() const final { return true; }

  bool IsZero() const final;
  void AccumulatePixelsAndPercent(
      const CSSToLengthConversionData& conversion_data,
      PixelsAndPercent& value,
      float multiplier) const final;
  double DoubleValue() const final;
  double ComputeLengthPx(
      const CSSToLengthConversionData& conversion_data) const final;
  void AccumulateLengthArray(CSSLengthArray& length_array,
                             double multiplier) const final;
  String CustomCSSText() const final;
  bool operator==(const CSSCalcExpressionNode& exp) const final;
  CSSPrimitiveValue::UnitType TypeWithCalcResolved() const final;
  void Trace(blink::Visitor* visitor) final;

 private:
  static CSSCalcExpressionNode* GetNumberSide(
      CSSCalcExpressionNode* left_side,
      CSSCalcExpressionNode* right_side);

  static String BuildCSSText(const String& left_expression,
                             const String& right_expression,
                             CSSMathOperator op);

  double Evaluate(double left_side, double right_side) const {
    return EvaluateOperator(left_side, right_side, operator_);
  }

  static double EvaluateOperator(double left_value,
                                 double right_value,
                                 CSSMathOperator op);

  const Member<CSSCalcExpressionNode> left_side_;
  const Member<CSSCalcExpressionNode> right_side_;
  const CSSMathOperator operator_;
};

template <>
struct DowncastTraits<CSSCalcBinaryOperation> {
  static bool AllowFrom(const CSSCalcExpressionNode& node) {
    return node.IsBinaryOperation();
  }
};

class CORE_EXPORT CSSCalcValue : public GarbageCollected<CSSCalcValue> {
 public:
  static CSSCalcValue* Create(const CSSParserTokenRange&, ValueRange);
  static CSSCalcValue* Create(CSSCalcExpressionNode*,
                              ValueRange = kValueRangeAll);

  static CSSCalcExpressionNode* CreateExpressionNode(CSSPrimitiveValue*,
                                                     bool is_integer = false);
  static CSSCalcExpressionNode* CreateExpressionNode(CSSCalcExpressionNode*,
                                                     CSSCalcExpressionNode*,
                                                     CSSMathOperator);
  static CSSCalcExpressionNode* CreateExpressionNode(double pixels,
                                                     double percent);

  CSSCalcValue(CSSCalcExpressionNode* expression, ValueRange range)
      : expression_(expression),
        non_negative_(range == kValueRangeNonNegative) {}

  scoped_refptr<CalculationValue> ToCalcValue(
      const CSSToLengthConversionData& conversion_data) const {
    PixelsAndPercent value(0, 0);
    expression_->AccumulatePixelsAndPercent(conversion_data, value);
    return CalculationValue::Create(
        value, non_negative_ ? kValueRangeNonNegative : kValueRangeAll);
  }
  CalculationCategory Category() const { return expression_->Category(); }
  bool IsInt() const { return expression_->IsInteger(); }
  double DoubleValue() const;
  bool IsNegative() const { return expression_->DoubleValue() < 0; }
  ValueRange PermittedValueRange() {
    return non_negative_ ? kValueRangeNonNegative : kValueRangeAll;
  }
  double ComputeLengthPx(const CSSToLengthConversionData&) const;
  void AccumulateLengthArray(CSSLengthArray& length_array,
                             double multiplier) const {
    expression_->AccumulateLengthArray(length_array, multiplier);
  }
  CSSCalcExpressionNode* ExpressionNode() const { return expression_.Get(); }

  String CustomCSSText() const;
  bool Equals(const CSSCalcValue&) const;

  void Trace(blink::Visitor* visitor) { visitor->Trace(expression_); }

 private:
  double ClampToPermittedRange(double) const;

  const Member<CSSCalcExpressionNode> expression_;
  const bool non_negative_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CALCULATION_VALUE_H_
