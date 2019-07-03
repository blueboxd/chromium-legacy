// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_MATH_FUNCTION_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_MATH_FUNCTION_VALUE_H_

#include "third_party/blink/renderer/core/css/css_primitive_value.h"

namespace blink {

// Numeric values that involve math functions (calc(), min(), max(), etc). This
// is the equivalence of CSS Typed OM's |CSSMathValue| in the |CSSValue| class
// hierarchy.
class CORE_EXPORT CSSMathFunctionValue : public CSSPrimitiveValue {
 public:
  static CSSMathFunctionValue* Create(const Length&, float zoom);
  static CSSMathFunctionValue* Create(CSSCalcValue*);

  explicit CSSMathFunctionValue(CSSCalcValue*);

  CSSCalcValue* CssCalcValue() const { return calc_; }

  void TraceAfterDispatch(blink::Visitor* visitor);

 private:
  Member<CSSCalcValue> calc_;
};

template <>
struct DowncastTraits<CSSMathFunctionValue> {
  static bool AllowFrom(const CSSValue& value) {
    return value.IsMathFunctionValue();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_MATH_FUNCTION_VALUE_H_
