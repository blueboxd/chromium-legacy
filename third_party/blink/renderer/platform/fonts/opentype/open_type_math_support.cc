// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/opentype/open_type_math_support.h"

// clang-format off
#include <hb.h>
#include <hb-ot.h>
// clang-format on

#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_face.h"

namespace {
// HarfBuzz' hb_position_t is a 16.16 fixed-point value.
float HarfBuzzUnitsToFloat(hb_position_t value) {
  static const float kFloatToHbRatio = 1.0f / (1 << 16);
  return kFloatToHbRatio * value;
}

}  // namespace

namespace blink {

bool OpenTypeMathSupport::HasMathData(const HarfBuzzFace* harfbuzz_face) {
  if (!harfbuzz_face)
    return false;

  hb_font_t* font =
      harfbuzz_face->GetScaledFont(nullptr, HarfBuzzFace::NoVerticalLayout);
  DCHECK(font);
  hb_face_t* face = hb_font_get_face(font);
  DCHECK(face);

  return hb_ot_math_has_data(face);
}

base::Optional<float> OpenTypeMathSupport::MathConstant(
    const HarfBuzzFace* harfbuzz_face,
    MathConstants constant) {
  if (!HasMathData(harfbuzz_face))
    return base::nullopt;

  hb_font_t* font =
      harfbuzz_face->GetScaledFont(nullptr, HarfBuzzFace::NoVerticalLayout);
  DCHECK(font);

  hb_position_t harfbuzz_value = hb_ot_math_get_constant(
      font, static_cast<hb_ot_math_constant_t>(constant));

  switch (constant) {
    case kScriptPercentScaleDown:
    case kScriptScriptPercentScaleDown:
    case kRadicalDegreeBottomRaisePercent:
      return base::Optional<float>(harfbuzz_value / 100.0);
    case kDelimitedSubFormulaMinHeight:
    case kDisplayOperatorMinHeight:
    case kMathLeading:
    case kAxisHeight:
    case kAccentBaseHeight:
    case kFlattenedAccentBaseHeight:
    case kSubscriptShiftDown:
    case kSubscriptTopMax:
    case kSubscriptBaselineDropMin:
    case kSuperscriptShiftUp:
    case kSuperscriptShiftUpCramped:
    case kSuperscriptBottomMin:
    case kSuperscriptBaselineDropMax:
    case kSubSuperscriptGapMin:
    case kSuperscriptBottomMaxWithSubscript:
    case kSpaceAfterScript:
    case kUpperLimitGapMin:
    case kUpperLimitBaselineRiseMin:
    case kLowerLimitGapMin:
    case kLowerLimitBaselineDropMin:
    case kStackTopShiftUp:
    case kStackTopDisplayStyleShiftUp:
    case kStackBottomShiftDown:
    case kStackBottomDisplayStyleShiftDown:
    case kStackGapMin:
    case kStackDisplayStyleGapMin:
    case kStretchStackTopShiftUp:
    case kStretchStackBottomShiftDown:
    case kStretchStackGapAboveMin:
    case kStretchStackGapBelowMin:
    case kFractionNumeratorShiftUp:
    case kFractionNumeratorDisplayStyleShiftUp:
    case kFractionDenominatorShiftDown:
    case kFractionDenominatorDisplayStyleShiftDown:
    case kFractionNumeratorGapMin:
    case kFractionNumDisplayStyleGapMin:
    case kFractionRuleThickness:
    case kFractionDenominatorGapMin:
    case kFractionDenomDisplayStyleGapMin:
    case kSkewedFractionHorizontalGap:
    case kSkewedFractionVerticalGap:
    case kOverbarVerticalGap:
    case kOverbarRuleThickness:
    case kOverbarExtraAscender:
    case kUnderbarVerticalGap:
    case kUnderbarRuleThickness:
    case kUnderbarExtraDescender:
    case kRadicalVerticalGap:
    case kRadicalDisplayStyleVerticalGap:
    case kRadicalRuleThickness:
    case kRadicalExtraAscender:
    case kRadicalKernBeforeDegree:
    case kRadicalKernAfterDegree:
      return base::Optional<float>(HarfBuzzUnitsToFloat(harfbuzz_value));
    default:
      NOTREACHED();
  }
  return base::nullopt;
}

}  // namespace blink
