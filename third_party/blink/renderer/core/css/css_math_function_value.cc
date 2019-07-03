// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_math_function_value.h"

#include "third_party/blink/renderer/core/css/css_calculation_value.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

struct SameSizeAsCSSMathFunctionValue : CSSPrimitiveValue {
  Member<void*> calc;
};
ASSERT_SIZE(CSSMathFunctionValue, SameSizeAsCSSMathFunctionValue);

void CSSMathFunctionValue::TraceAfterDispatch(blink::Visitor* visitor) {
  visitor->Trace(calc_);
  CSSPrimitiveValue::TraceAfterDispatch(visitor);
}

CSSMathFunctionValue::CSSMathFunctionValue(CSSCalcValue* calc)
    : CSSPrimitiveValue(UnitType::kCalc, kMathFunctionClass), calc_(calc) {}

// static
CSSMathFunctionValue* CSSMathFunctionValue::Create(CSSCalcValue* calc) {
  return MakeGarbageCollected<CSSMathFunctionValue>(calc);
}

// static
CSSMathFunctionValue* CSSMathFunctionValue::Create(const Length& length,
                                                   float zoom) {
  const CalculationValue& calc = length.GetCalculationValue();
  return Create(CSSCalcValue::Create(
      CSSCalcValue::CreateExpressionNode(calc.Pixels() / zoom, calc.Percent()),
      calc.GetValueRange()));
}

}  // namespace blink
