// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/opentype/open_type_math_support.h"
#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/opentype/open_type_types.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class OpenTypeMathSupportTest : public testing::Test {
 protected:
  void SetUp() override {
    font_description.SetComputedSize(10.0);
    font = Font(font_description);
    font.Update(nullptr);
  }

  void TearDown() override {}

  Font CreateMathFont(const String& name, float size = 1000) {
    FontDescription::VariantLigatures ligatures;
    return blink::test::CreateTestFont(
        "MathTestFont",
        blink::test::BlinkWebTestsFontsTestDataPath(String("math/") + name),
        size, &ligatures);
  }

  bool HasMathData(const String& name) {
    return OpenTypeMathSupport::HasMathData(
        CreateMathFont(name).PrimaryFont()->PlatformData().GetHarfBuzzFace());
  }

  base::Optional<float> MathConstant(
      const String& name,
      OpenTypeMathSupport::MathConstants constant) {
    Font math = CreateMathFont(name);
    return OpenTypeMathSupport::MathConstant(
        math.PrimaryFont()->PlatformData().GetHarfBuzzFace(), constant);
  }

  FontDescription font_description;
  Font font;
};

TEST_F(OpenTypeMathSupportTest, HasMathData) {
  // Null parameter.
  EXPECT_FALSE(OpenTypeMathSupport::HasMathData(nullptr));

  // Font without a MATH table.
  EXPECT_FALSE(HasMathData("math-text.woff"));

  // Font with a MATH table.
  EXPECT_TRUE(HasMathData("axisheight5000-verticalarrow14000.woff"));
}

TEST_F(OpenTypeMathSupportTest, MathConstantNullOpt) {
  Font math_text = CreateMathFont("math-text.woff");

  for (int i = OpenTypeMathSupport::MathConstants::kScriptPercentScaleDown;
       i <=
       OpenTypeMathSupport::MathConstants::kRadicalDegreeBottomRaisePercent;
       i++) {
    auto math_constant = static_cast<OpenTypeMathSupport::MathConstants>(i);

    // Null parameter.
    EXPECT_FALSE(OpenTypeMathSupport::MathConstant(nullptr, math_constant));

    // Font without a MATH table.
    EXPECT_FALSE(OpenTypeMathSupport::MathConstant(
        math_text.PrimaryFont()->PlatformData().GetHarfBuzzFace(),
        math_constant));
  }
}

// See third_party/blink/web_tests/external/wpt/mathml/tools/percentscaledown.py
TEST_F(OpenTypeMathSupportTest, MathConstantPercentScaleDown) {
  {
    auto result = MathConstant(
        "scriptpercentscaledown80-scriptscriptpercentscaledown0.woff",
        OpenTypeMathSupport::MathConstants::kScriptPercentScaleDown);
    EXPECT_TRUE(result);
    EXPECT_FLOAT_EQ(*result, .8);
  }

  {
    auto result = MathConstant(
        "scriptpercentscaledown0-scriptscriptpercentscaledown40.woff",
        OpenTypeMathSupport::MathConstants::kScriptScriptPercentScaleDown);
    EXPECT_TRUE(result);
    EXPECT_FLOAT_EQ(*result, .4);
  }
}

// See third_party/blink/web_tests/external/wpt/mathml/tools/fractions.py
TEST_F(OpenTypeMathSupportTest, MathConstantFractions) {
  {
    auto result = MathConstant(
        "fraction-numeratorshiftup11000-axisheight1000-rulethickness1000.woff",
        OpenTypeMathSupport::MathConstants::kFractionNumeratorShiftUp);
    EXPECT_TRUE(result);
    EXPECT_FLOAT_EQ(*result, 11000);
  }

  {
    auto result = MathConstant(
        "fraction-numeratordisplaystyleshiftup2000-axisheight1000-"
        "rulethickness1000.woff",
        OpenTypeMathSupport::MathConstants::
            kFractionNumeratorDisplayStyleShiftUp);
    EXPECT_TRUE(result);
    EXPECT_FLOAT_EQ(*result, 2000);
  }

  {
    auto result = MathConstant(
        "fraction-denominatorshiftdown3000-axisheight1000-rulethickness1000."
        "woff",
        OpenTypeMathSupport::MathConstants::kFractionDenominatorShiftDown);
    EXPECT_TRUE(result);
    EXPECT_FLOAT_EQ(*result, 3000);
  }

  {
    auto result = MathConstant(
        "fraction-denominatordisplaystyleshiftdown6000-axisheight1000-"
        "rulethickness1000.woff",
        OpenTypeMathSupport::MathConstants::
            kFractionDenominatorDisplayStyleShiftDown);
    EXPECT_TRUE(result);
    EXPECT_FLOAT_EQ(*result, 6000);
  }

  {
    auto result = MathConstant(
        "fraction-numeratorgapmin9000-rulethickness1000.woff",
        OpenTypeMathSupport::MathConstants::kFractionNumeratorGapMin);
    EXPECT_TRUE(result);
    EXPECT_FLOAT_EQ(*result, 9000);
  }

  {
    auto result = MathConstant(
        "fraction-numeratordisplaystylegapmin8000-rulethickness1000.woff",
        OpenTypeMathSupport::MathConstants::kFractionNumDisplayStyleGapMin);
    EXPECT_TRUE(result);
    EXPECT_FLOAT_EQ(*result, 8000);
  }

  {
    auto result = MathConstant(
        "fraction-rulethickness10000.woff",
        OpenTypeMathSupport::MathConstants::kFractionRuleThickness);
    EXPECT_TRUE(result);
    EXPECT_FLOAT_EQ(*result, 10000);
  }

  {
    auto result = MathConstant(
        "fraction-denominatorgapmin4000-rulethickness1000.woff",
        OpenTypeMathSupport::MathConstants::kFractionDenominatorGapMin);
    EXPECT_TRUE(result);
    EXPECT_FLOAT_EQ(*result, 4000);
  }

  {
    auto result = MathConstant(
        "fraction-denominatordisplaystylegapmin5000-rulethickness1000.woff",
        OpenTypeMathSupport::MathConstants::kFractionDenomDisplayStyleGapMin);
    EXPECT_TRUE(result);
    EXPECT_FLOAT_EQ(*result, 5000);
  }
}

// See third_party/blink/web_tests/external/wpt/mathml/tools/radicals.py
TEST_F(OpenTypeMathSupportTest, MathConstantRadicals) {
  {
    auto result = MathConstant(
        "radical-degreebottomraisepercent25-rulethickness1000.woff",
        OpenTypeMathSupport::MathConstants::kRadicalDegreeBottomRaisePercent);
    EXPECT_TRUE(result);
    EXPECT_FLOAT_EQ(*result, .25);
  }

  {
    auto result =
        MathConstant("radical-verticalgap6000-rulethickness1000.woff",
                     OpenTypeMathSupport::MathConstants::kRadicalVerticalGap);
    EXPECT_TRUE(result);
    EXPECT_FLOAT_EQ(*result, 6000);
  }

  {
    auto result = MathConstant(
        "radical-displaystyleverticalgap7000-rulethickness1000.woff",
        OpenTypeMathSupport::MathConstants::kRadicalDisplayStyleVerticalGap);
    EXPECT_TRUE(result);
    EXPECT_FLOAT_EQ(*result, 7000);
  }

  {
    auto result =
        MathConstant("radical-rulethickness8000.woff",
                     OpenTypeMathSupport::MathConstants::kRadicalRuleThickness);
    EXPECT_TRUE(result);
    EXPECT_FLOAT_EQ(*result, 8000);
  }

  {
    auto result =
        MathConstant("radical-extraascender3000-rulethickness1000.woff",
                     OpenTypeMathSupport::MathConstants::kRadicalExtraAscender);
    EXPECT_TRUE(result);
    EXPECT_FLOAT_EQ(*result, 3000);
  }

  {
    auto result = MathConstant(
        "radical-kernbeforedegree4000-rulethickness1000.woff",
        OpenTypeMathSupport::MathConstants::kRadicalKernBeforeDegree);
    EXPECT_TRUE(result);
    EXPECT_FLOAT_EQ(*result, 4000);
  }

  {
    auto result = MathConstant(
        "radical-kernafterdegreeminus5000-rulethickness1000.woff",
        OpenTypeMathSupport::MathConstants::kRadicalKernAfterDegree);
    EXPECT_TRUE(result);
    EXPECT_FLOAT_EQ(*result, -5000);
  }
}

}  // namespace blink
