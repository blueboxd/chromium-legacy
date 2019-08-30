// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_primitive_value.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_math_expression_node.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"

namespace blink {
namespace {

using UnitType = CSSPrimitiveValue::UnitType;

struct UnitValue {
  double value;
  UnitType unit_type;
};

CSSNumericLiteralValue* Create(UnitValue v) {
  return CSSNumericLiteralValue::Create(v.value, v.unit_type);
}

CSSPrimitiveValue* CreateAddition(UnitValue a, UnitValue b) {
  return CSSMathFunctionValue::Create(CSSMathExpressionBinaryOperation::Create(
      CSSMathExpressionNumericLiteral::Create(Create(a)),
      CSSMathExpressionNumericLiteral::Create(Create(b)),
      CSSMathOperator::kAdd));
}

CSSPrimitiveValue* CreateNonNegativeSubtraction(UnitValue a, UnitValue b) {
  return CSSMathFunctionValue::Create(
      CSSMathExpressionBinaryOperation::Create(
          CSSMathExpressionNumericLiteral::Create(Create(a)),
          CSSMathExpressionNumericLiteral::Create(Create(b)),
          CSSMathOperator::kSubtract),
      kValueRangeNonNegative);
}

TEST(CSSPrimitiveValueTest, IsTime) {
  EXPECT_FALSE(Create({5.0, UnitType::kNumber})->IsTime());
  EXPECT_FALSE(Create({5.0, UnitType::kDegrees})->IsTime());
  EXPECT_TRUE(Create({5.0, UnitType::kSeconds})->IsTime());
  EXPECT_TRUE(Create({5.0, UnitType::kMilliseconds})->IsTime());
}

TEST(CSSPrimitiveValueTest, IsTimeCalc) {
  {
    UnitValue a = {1.0, UnitType::kSeconds};
    UnitValue b = {1000.0, UnitType::kMilliseconds};
    EXPECT_TRUE(CreateAddition(a, b)->IsTime());
  }
  {
    UnitValue a = {1.0, UnitType::kDegrees};
    UnitValue b = {1000.0, UnitType::kGradians};
    EXPECT_FALSE(CreateAddition(a, b)->IsTime());
  }
}

TEST(CSSPrimitiveValueTest, ClampTimeToNonNegative) {
  UnitValue a = {4926, UnitType::kMilliseconds};
  UnitValue b = {5, UnitType::kSeconds};
  EXPECT_EQ(0.0, CreateNonNegativeSubtraction(a, b)->ComputeSeconds());
}

TEST(CSSPrimitiveValueTest, ClampAngleToNonNegative) {
  UnitValue a = {89, UnitType::kDegrees};
  UnitValue b = {0.25, UnitType::kTurns};
  EXPECT_EQ(0.0, CreateNonNegativeSubtraction(a, b)->ComputeDegrees());
}

TEST(CSSPrimitiveValueTest, IsResolution) {
  EXPECT_FALSE(Create({5.0, UnitType::kNumber})->IsResolution());
  EXPECT_FALSE(Create({5.0, UnitType::kDegrees})->IsResolution());
  EXPECT_TRUE(Create({5.0, UnitType::kDotsPerPixel})->IsResolution());
  EXPECT_TRUE(Create({5.0, UnitType::kDotsPerCentimeter})->IsResolution());
}

}  // namespace
}  // namespace blink
