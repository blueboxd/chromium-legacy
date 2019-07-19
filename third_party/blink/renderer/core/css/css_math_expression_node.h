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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_MATH_EXPRESSION_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_MATH_EXPRESSION_NODE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_math_operator.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/platform/geometry/calculation_value.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class CSSNumericLiteralValue;

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

class CORE_EXPORT CSSMathExpressionNode
    : public GarbageCollected<CSSMathExpressionNode> {
 public:
  static CSSMathExpressionNode* CreateFromPixelsAndPercent(double pixels,
                                                           double percent);
  static CSSMathExpressionNode* ParseCalc(const CSSParserTokenRange& tokens);

  virtual bool IsNumericLiteral() const { return false; }
  virtual bool IsBinaryOperation() const { return false; }

  virtual bool IsZero() const = 0;

  // Resolves the expression into one value *without doing any type conversion*.
  // Hits DCHECK if type conversion is required.
  virtual double DoubleValue() const = 0;

  virtual double ComputeLengthPx(const CSSToLengthConversionData&) const = 0;
  virtual void AccumulateLengthArray(CSSLengthArray&,
                                     double multiplier) const = 0;
  virtual void AccumulatePixelsAndPercent(const CSSToLengthConversionData&,
                                          PixelsAndPercent&,
                                          float multiplier = 1) const = 0;

  // Evaluates the expression with type conversion (e.g., cm -> px) handled, and
  // returns the result value in the canonical unit of the corresponding
  // category (see https://www.w3.org/TR/css3-values/#canonical-unit).
  // TODO(crbug.com/984372): We currently use 'ms' as the canonical unit of
  // <time>. Switch to 's' to follow the spec.
  // Returns |nullopt| on evaluation failures due to the following reasons:
  // - The category doesn't have a canonical unit (e.g., |kCalcPercentLength|).
  // - A type conversion that doesn't have a fixed conversion ratio is needed
  //   (e.g., between 'px' and 'em').
  // - There's an unsupported calculation, e.g., dividing two lengths.
  virtual base::Optional<double> ComputeValueInCanonicalUnit() const = 0;

  virtual String CustomCSSText() const = 0;
  virtual bool operator==(const CSSMathExpressionNode& other) const {
    return category_ == other.category_ && is_integer_ == other.is_integer_;
  }

  virtual bool IsComputationallyIndependent() const = 0;

  CalculationCategory Category() const { return category_; }

  // Returns the unit type of the math expression *without doing any type
  // conversion* (e.g., 1px + 1em needs type conversion to resolve).
  // Returns |UnitType::kUnknown| if type conversion is required.
  virtual CSSPrimitiveValue::UnitType ResolvedUnitType() const = 0;

  bool IsInteger() const { return is_integer_; }

  bool IsNestedCalc() const { return is_nested_calc_; }
  void SetIsNestedCalc() { is_nested_calc_ = true; }

  virtual void Trace(blink::Visitor* visitor) {}

 protected:
  CSSMathExpressionNode(CalculationCategory category, bool is_integer)
      : category_(category), is_integer_(is_integer) {
    DCHECK_NE(category, kCalcOther);
  }

  CalculationCategory category_;
  bool is_integer_;
  bool is_nested_calc_ = false;
};

class CORE_EXPORT CSSMathExpressionNumericLiteral final
    : public CSSMathExpressionNode {
 public:
  static CSSMathExpressionNumericLiteral* Create(CSSNumericLiteralValue* value,
                                                 bool is_integer = false);
  static CSSMathExpressionNumericLiteral*
  Create(double value, CSSPrimitiveValue::UnitType type, bool is_integer);

  CSSMathExpressionNumericLiteral(CSSNumericLiteralValue* value,
                                  bool is_integer);

  bool IsNumericLiteral() const final { return true; }

  bool IsZero() const final;
  String CustomCSSText() const final;
  void AccumulatePixelsAndPercent(
      const CSSToLengthConversionData& conversion_data,
      PixelsAndPercent& value,
      float multiplier) const final;
  double DoubleValue() const final;
  base::Optional<double> ComputeValueInCanonicalUnit() const final;
  double ComputeLengthPx(
      const CSSToLengthConversionData& conversion_data) const final;
  void AccumulateLengthArray(CSSLengthArray& length_array,
                             double multiplier) const final;
  bool IsComputationallyIndependent() const final;
  bool operator==(const CSSMathExpressionNode& other) const final;
  CSSPrimitiveValue::UnitType ResolvedUnitType() const final;
  void Trace(blink::Visitor* visitor) final;

 private:
  Member<CSSNumericLiteralValue> value_;
};

template <>
struct DowncastTraits<CSSMathExpressionNumericLiteral> {
  static bool AllowFrom(const CSSMathExpressionNode& node) {
    return node.IsNumericLiteral();
  }
};

class CORE_EXPORT CSSMathExpressionBinaryOperation final
    : public CSSMathExpressionNode {
 public:
  static CSSMathExpressionNode* Create(CSSMathExpressionNode* left_side,
                                       CSSMathExpressionNode* right_side,
                                       CSSMathOperator op);
  static CSSMathExpressionNode* CreateSimplified(
      CSSMathExpressionNode* left_side,
      CSSMathExpressionNode* right_side,
      CSSMathOperator op);

  CSSMathExpressionBinaryOperation(CSSMathExpressionNode* left_side,
                                   CSSMathExpressionNode* right_side,
                                   CSSMathOperator op,
                                   CalculationCategory category);

  const CSSMathExpressionNode* LeftExpressionNode() const { return left_side_; }
  const CSSMathExpressionNode* RightExpressionNode() const {
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
  base::Optional<double> ComputeValueInCanonicalUnit() const final;
  double ComputeLengthPx(
      const CSSToLengthConversionData& conversion_data) const final;
  void AccumulateLengthArray(CSSLengthArray& length_array,
                             double multiplier) const final;
  bool IsComputationallyIndependent() const final;
  String CustomCSSText() const final;
  bool operator==(const CSSMathExpressionNode& exp) const final;
  CSSPrimitiveValue::UnitType ResolvedUnitType() const final;
  void Trace(blink::Visitor* visitor) final;

 private:
  static CSSMathExpressionNode* GetNumberSide(
      CSSMathExpressionNode* left_side,
      CSSMathExpressionNode* right_side);

  static String BuildCSSText(const String& left_expression,
                             const String& right_expression,
                             CSSMathOperator op);

  double Evaluate(double left_side, double right_side) const {
    return EvaluateOperator(left_side, right_side, operator_);
  }

  static double EvaluateOperator(double left_value,
                                 double right_value,
                                 CSSMathOperator op);

  const Member<CSSMathExpressionNode> left_side_;
  const Member<CSSMathExpressionNode> right_side_;
  const CSSMathOperator operator_;
};

template <>
struct DowncastTraits<CSSMathExpressionBinaryOperation> {
  static bool AllowFrom(const CSSMathExpressionNode& node) {
    return node.IsBinaryOperation();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_MATH_EXPRESSION_NODE_H_
