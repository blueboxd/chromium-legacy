// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_MATH_FUNCTION_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_MATH_FUNCTION_VALUE_H_

#include "third_party/blink/renderer/core/css/css_math_expression_node.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"

namespace blink {

// Numeric values that involve math functions (calc(), min(), max(), etc). This
// is the equivalence of CSS Typed OM's |CSSMathValue| in the |CSSValue| class
// hierarchy.
class CORE_EXPORT CSSMathFunctionValue : public CSSPrimitiveValue {
 public:
  static CSSMathFunctionValue* Create(const Length&, float zoom);
  static CSSMathFunctionValue* Create(CSSMathExpressionNode*,
                                      ValueRange = kValueRangeAll);

  CSSMathFunctionValue(CSSMathExpressionNode* expression, ValueRange range);

  CSSMathExpressionNode* ExpressionNode() const { return expression_; }

  scoped_refptr<CalculationValue> ToCalcValue(
      const CSSToLengthConversionData& conversion_data) const {
    PixelsAndPercent value(0, 0);
    expression_->AccumulatePixelsAndPercent(conversion_data, value);
    return CalculationValue::Create(value, PermittedValueRange());
  }

  UnitType TypeWithMathFunctionResolved() const;
  bool MayHaveRelativeUnit() const;

  CalculationCategory Category() const { return expression_->Category(); }
  bool IsInt() const { return expression_->IsInteger(); }
  bool IsNegative() const { return expression_->DoubleValue() < 0; }
  ValueRange PermittedValueRange() const {
    return IsNonNegative() ? kValueRangeNonNegative : kValueRangeAll;
  }

  bool IsZero() const;

  // TODO(crbug.com/979895): Make sure these functions are called only when
  // the math expression resolves into a double value.
  double DoubleValue() const;
  double ComputeSeconds() const;
  double ComputeDegrees() const;
  double ComputeLengthPx(
      const CSSToLengthConversionData& conversion_data) const;

  void AccumulateLengthArray(CSSLengthArray& length_array,
                             double multiplier) const;
  Length ConvertToLength(
      const CSSToLengthConversionData& conversion_data) const;

  String CustomCSSText() const;
  bool Equals(const CSSMathFunctionValue& other) const;

  void TraceAfterDispatch(blink::Visitor* visitor);

 private:
  bool IsNonNegative() const { return is_non_negative_math_function_; }

  double ClampToPermittedRange(double) const;

  Member<CSSMathExpressionNode> expression_;
};

template <>
struct DowncastTraits<CSSMathFunctionValue> {
  static bool AllowFrom(const CSSValue& value) {
    return value.IsMathFunctionValue();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_MATH_FUNCTION_VALUE_H_
