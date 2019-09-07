// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/interpolable_length.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

using UnitType = CSSPrimitiveValue::UnitType;

// Verifies the "shape" of the |InterpolableLength| after various operations.
// Subject to change if the internal representation of |InterpolableLength| is
// changed.
class InterpolableLengthTest : public ::testing::Test {
 protected:
  std::unique_ptr<InterpolableLength> Create(double value,
                                             UnitType type) const {
    return std::make_unique<InterpolableLength>(value, type);
  }

  std::unique_ptr<InterpolableLength> Interpolate(
      const InterpolableLength& from,
      const InterpolableLength& to,
      double progress) {
    std::unique_ptr<InterpolableLength> result =
        InterpolableLength::CreateNeutral();
    from.Interpolate(to, progress, *result);
    return result;
  }

  bool IsNumericLiteral(const InterpolableLength& length) const {
    return length.IsNumericLiteral();
  }

  double GetSingleValue(const InterpolableLength& length) const {
    DCHECK(length.IsNumericLiteral());
    return length.single_value_;
  }

  UnitType GetUnitType(const InterpolableLength& length) const {
    DCHECK(length.IsNumericLiteral());
    return length.unit_type_;
  }

  bool IsExpression(const InterpolableLength& length) const {
    return length.IsExpression();
  }
};

TEST_F(InterpolableLengthTest, InterpolateSameUnits) {
  {
    auto length_a = InterpolableLength::CreatePixels(4);
    auto length_b = InterpolableLength::CreatePixels(6);
    auto result = Interpolate(*length_a, *length_b, 0.5);

    EXPECT_TRUE(IsNumericLiteral(*result));
    EXPECT_EQ(5, GetSingleValue(*result));
    EXPECT_EQ(UnitType::kPixels, GetUnitType(*result));
    EXPECT_FALSE(result->HasPercentage());
  }

  {
    auto length_a = Create(4, UnitType::kEms);
    auto length_b = Create(6, UnitType::kEms);
    auto result = Interpolate(*length_a, *length_b, 0.5);

    EXPECT_TRUE(IsNumericLiteral(*result));
    EXPECT_EQ(5, GetSingleValue(*result));
    EXPECT_EQ(UnitType::kEms, GetUnitType(*result));
    EXPECT_FALSE(result->HasPercentage());
  }

  {
    auto length_a = InterpolableLength::CreatePercent(4);
    auto length_b = InterpolableLength::CreatePercent(6);
    auto result = Interpolate(*length_a, *length_b, 0.5);

    EXPECT_TRUE(IsNumericLiteral(*result));
    EXPECT_EQ(5, GetSingleValue(*result));
    EXPECT_EQ(UnitType::kPercentage, GetUnitType(*result));
    EXPECT_TRUE(result->HasPercentage());
  }
}

TEST_F(InterpolableLengthTest, InterpolateIncompatibleUnits) {
  {
    auto length_a = InterpolableLength::CreatePixels(4);
    auto length_b = Create(6, UnitType::kEms);
    auto result = Interpolate(*length_a, *length_b, 0.5);

    EXPECT_TRUE(IsExpression(*result));
    EXPECT_FALSE(result->HasPercentage());
    EXPECT_EQ("calc(2px + 3em)",
              result->CreateCSSValue(kValueRangeAll)->CustomCSSText());
  }
}

TEST_F(InterpolableLengthTest, SetHasPercentage) {
  {
    auto length = InterpolableLength::CreateNeutral();
    length->SetHasPercentage();

    EXPECT_TRUE(length->HasPercentage());
    EXPECT_TRUE(IsNumericLiteral(*length));
    EXPECT_EQ(0, GetSingleValue(*length));
    EXPECT_EQ(UnitType::kPercentage, GetUnitType(*length));
  }

  {
    auto length = InterpolableLength::CreatePercent(10);
    length->SetHasPercentage();

    EXPECT_TRUE(length->HasPercentage());
    EXPECT_TRUE(IsNumericLiteral(*length));
    EXPECT_EQ(10, GetSingleValue(*length));
    EXPECT_EQ(UnitType::kPercentage, GetUnitType(*length));
  }

  {
    auto length = InterpolableLength::CreatePixels(10);
    length->SetHasPercentage();

    EXPECT_TRUE(length->HasPercentage());
    EXPECT_TRUE(IsExpression(*length));
    EXPECT_EQ("calc(10px + 0%)",
              length->CreateCSSValue(kValueRangeAll)->CustomCSSText());
  }
}

}  // namespace blink
