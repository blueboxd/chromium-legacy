/*
 * Copyright (C) 2011 Andreas Kling (kling@webkit.org)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "third_party/blink/renderer/core/css/css_value.h"

#include "third_party/blink/renderer/core/css/css_axis_value.h"
#include "third_party/blink/renderer/core/css/css_basic_shape_values.h"
#include "third_party/blink/renderer/core/css/css_border_image_slice_value.h"
#include "third_party/blink/renderer/core/css/css_color_value.h"
#include "third_party/blink/renderer/core/css/css_content_distribution_value.h"
#include "third_party/blink/renderer/core/css/css_counter_value.h"
#include "third_party/blink/renderer/core/css/css_crossfade_value.h"
#include "third_party/blink/renderer/core/css/css_cursor_image_value.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/css_font_face_src_value.h"
#include "third_party/blink/renderer/core/css/css_font_family_value.h"
#include "third_party/blink/renderer/core/css/css_font_feature_value.h"
#include "third_party/blink/renderer/core/css/css_font_style_range_value.h"
#include "third_party/blink/renderer/core/css/css_font_variation_value.h"
#include "third_party/blink/renderer/core/css/css_function_value.h"
#include "third_party/blink/renderer/core/css/css_gradient_value.h"
#include "third_party/blink/renderer/core/css/css_grid_auto_repeat_value.h"
#include "third_party/blink/renderer/core/css/css_grid_integer_repeat_value.h"
#include "third_party/blink/renderer/core/css/css_grid_line_names_value.h"
#include "third_party/blink/renderer/core/css/css_grid_template_areas_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_image_set_value.h"
#include "third_party/blink/renderer/core/css/css_image_value.h"
#include "third_party/blink/renderer/core/css/css_inherited_value.h"
#include "third_party/blink/renderer/core/css/css_initial_value.h"
#include "third_party/blink/renderer/core/css/css_invalid_variable_value.h"
#include "third_party/blink/renderer/core/css/css_keyframe_shorthand_value.h"
#include "third_party/blink/renderer/core/css/css_layout_function_value.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_paint_value.h"
#include "third_party/blink/renderer/core/css/css_path_value.h"
#include "third_party/blink/renderer/core/css/css_pending_interpolation_value.h"
#include "third_party/blink/renderer/core/css/css_pending_substitution_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_quad_value.h"
#include "third_party/blink/renderer/core/css/css_ray_value.h"
#include "third_party/blink/renderer/core/css/css_reflect_value.h"
#include "third_party/blink/renderer/core/css/css_shadow_value.h"
#include "third_party/blink/renderer/core/css/css_string_value.h"
#include "third_party/blink/renderer/core/css/css_timing_function_value.h"
#include "third_party/blink/renderer/core/css/css_unicode_range_value.h"
#include "third_party/blink/renderer/core/css/css_unset_value.h"
#include "third_party/blink/renderer/core/css/css_uri_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/core/css/css_variable_reference_value.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

using namespace cssvalue;

struct SameSizeAsCSSValue
    : public GarbageCollectedFinalized<SameSizeAsCSSValue> {
  uint32_t bitfields;
};
ASSERT_SIZE(CSSValue, SameSizeAsCSSValue);

CSSValue* CSSValue::Create(const Length& value, float zoom) {
  switch (value.GetType()) {
    case Length::kAuto:
    case Length::kMinContent:
    case Length::kMaxContent:
    case Length::kFillAvailable:
    case Length::kFitContent:
    case Length::kExtendToZoom:
      return CSSIdentifierValue::Create(value);
    case Length::kPercent:
    case Length::kFixed:
    case Length::kCalculated:
      return CSSPrimitiveValue::CreateFromLength(value, zoom);
    case Length::kDeviceWidth:
    case Length::kDeviceHeight:
    case Length::kMaxSizeNone:
      NOTREACHED();
      break;
  }
  return nullptr;
}

bool CSSValue::HasFailedOrCanceledSubresources() const {
  if (IsValueList())
    return To<CSSValueList>(this)->HasFailedOrCanceledSubresources();
  if (GetClassType() == kFontFaceSrcClass)
    return To<CSSFontFaceSrcValue>(this)->HasFailedOrCanceledSubresources();
  if (GetClassType() == kImageClass)
    return To<CSSImageValue>(this)->HasFailedOrCanceledSubresources();
  if (GetClassType() == kCrossfadeClass)
    return To<CSSCrossfadeValue>(this)->HasFailedOrCanceledSubresources();
  if (GetClassType() == kImageSetClass)
    return To<CSSImageSetValue>(this)->HasFailedOrCanceledSubresources();

  return false;
}

bool CSSValue::MayContainUrl() const {
  if (IsValueList())
    return To<CSSValueList>(*this).MayContainUrl();
  return IsImageValue() || IsURIValue();
}

void CSSValue::ReResolveUrl(const Document& document) const {
  // TODO(fs): Should handle all values that can contain URLs.
  if (IsImageValue()) {
    To<CSSImageValue>(*this).ReResolveURL(document);
    return;
  }
  if (IsURIValue()) {
    To<CSSURIValue>(*this).ReResolveUrl(document);
    return;
  }
  if (IsValueList()) {
    To<CSSValueList>(*this).ReResolveUrl(document);
    return;
  }
}

template <class ChildClassType>
inline static bool CompareCSSValues(const CSSValue& first,
                                    const CSSValue& second) {
  return static_cast<const ChildClassType&>(first).Equals(
      static_cast<const ChildClassType&>(second));
}

bool CSSValue::operator==(const CSSValue& other) const {
  if (class_type_ == other.class_type_) {
    switch (GetClassType()) {
      case kAxisClass:
        return CompareCSSValues<CSSAxisValue>(*this, other);
      case kBasicShapeCircleClass:
        return CompareCSSValues<CSSBasicShapeCircleValue>(*this, other);
      case kBasicShapeEllipseClass:
        return CompareCSSValues<CSSBasicShapeEllipseValue>(*this, other);
      case kBasicShapePolygonClass:
        return CompareCSSValues<CSSBasicShapePolygonValue>(*this, other);
      case kBasicShapeInsetClass:
        return CompareCSSValues<CSSBasicShapeInsetValue>(*this, other);
      case kBorderImageSliceClass:
        return CompareCSSValues<CSSBorderImageSliceValue>(*this, other);
      case kColorClass:
        return CompareCSSValues<CSSColorValue>(*this, other);
      case kCounterClass:
        return CompareCSSValues<CSSCounterValue>(*this, other);
      case kCursorImageClass:
        return CompareCSSValues<CSSCursorImageValue>(*this, other);
      case kFontFaceSrcClass:
        return CompareCSSValues<CSSFontFaceSrcValue>(*this, other);
      case kFontFamilyClass:
        return CompareCSSValues<CSSFontFamilyValue>(*this, other);
      case kFontFeatureClass:
        return CompareCSSValues<cssvalue::CSSFontFeatureValue>(*this, other);
      case kFontStyleRangeClass:
        return CompareCSSValues<CSSFontStyleRangeValue>(*this, other);
      case kFontVariationClass:
        return CompareCSSValues<CSSFontVariationValue>(*this, other);
      case kFunctionClass:
        return CompareCSSValues<CSSFunctionValue>(*this, other);
      case kLayoutFunctionClass:
        return CompareCSSValues<CSSLayoutFunctionValue>(*this, other);
      case kLinearGradientClass:
        return CompareCSSValues<CSSLinearGradientValue>(*this, other);
      case kRadialGradientClass:
        return CompareCSSValues<CSSRadialGradientValue>(*this, other);
      case kConicGradientClass:
        return CompareCSSValues<CSSConicGradientValue>(*this, other);
      case kCrossfadeClass:
        return CompareCSSValues<CSSCrossfadeValue>(*this, other);
      case kPaintClass:
        return CompareCSSValues<CSSPaintValue>(*this, other);
      case kCustomIdentClass:
        return CompareCSSValues<CSSCustomIdentValue>(*this, other);
      case kImageClass:
        return CompareCSSValues<CSSImageValue>(*this, other);
      case kInheritedClass:
        return CompareCSSValues<CSSInheritedValue>(*this, other);
      case kInitialClass:
        return CompareCSSValues<CSSInitialValue>(*this, other);
      case kUnsetClass:
        return CompareCSSValues<CSSUnsetValue>(*this, other);
      case kGridAutoRepeatClass:
        return CompareCSSValues<CSSGridAutoRepeatValue>(*this, other);
      case kGridIntegerRepeatClass:
        return CompareCSSValues<CSSGridIntegerRepeatValue>(*this, other);
      case kGridLineNamesClass:
        return CompareCSSValues<CSSGridLineNamesValue>(*this, other);
      case kGridTemplateAreasClass:
        return CompareCSSValues<CSSGridTemplateAreasValue>(*this, other);
      case kPathClass:
        return CompareCSSValues<CSSPathValue>(*this, other);
      case kNumericLiteralClass:
        return CompareCSSValues<CSSNumericLiteralValue>(*this, other);
      case kMathFunctionClass:
        return CompareCSSValues<CSSMathFunctionValue>(*this, other);
      case kRayClass:
        return CompareCSSValues<CSSRayValue>(*this, other);
      case kIdentifierClass:
        return CompareCSSValues<CSSIdentifierValue>(*this, other);
      case kKeyframeShorthandClass:
        return CompareCSSValues<CSSKeyframeShorthandValue>(*this, other);
      case kQuadClass:
        return CompareCSSValues<CSSQuadValue>(*this, other);
      case kReflectClass:
        return CompareCSSValues<CSSReflectValue>(*this, other);
      case kShadowClass:
        return CompareCSSValues<CSSShadowValue>(*this, other);
      case kStringClass:
        return CompareCSSValues<CSSStringValue>(*this, other);
      case kCubicBezierTimingFunctionClass:
        return CompareCSSValues<CSSCubicBezierTimingFunctionValue>(*this,
                                                                   other);
      case kStepsTimingFunctionClass:
        return CompareCSSValues<CSSStepsTimingFunctionValue>(*this, other);
      case kUnicodeRangeClass:
        return CompareCSSValues<CSSUnicodeRangeValue>(*this, other);
      case kURIClass:
        return CompareCSSValues<CSSURIValue>(*this, other);
      case kValueListClass:
        return CompareCSSValues<CSSValueList>(*this, other);
      case kValuePairClass:
        return CompareCSSValues<CSSValuePair>(*this, other);
      case kImageSetClass:
        return CompareCSSValues<CSSImageSetValue>(*this, other);
      case kCSSContentDistributionClass:
        return CompareCSSValues<CSSContentDistributionValue>(*this, other);
      case kPendingInterpolationClass:
        return CompareCSSValues<CSSPendingInterpolationValue>(*this, other);
      case kCustomPropertyDeclarationClass:
        return CompareCSSValues<CSSCustomPropertyDeclaration>(*this, other);
      case kVariableReferenceClass:
        return CompareCSSValues<CSSVariableReferenceValue>(*this, other);
      case kPendingSubstitutionValueClass:
        return CompareCSSValues<CSSPendingSubstitutionValue>(*this, other);
      case kInvalidVariableValueClass:
        return CompareCSSValues<CSSInvalidVariableValue>(*this, other);
    }
    NOTREACHED();
    return false;
  }
  return false;
}

String CSSValue::CssText() const {
  switch (GetClassType()) {
    case kAxisClass:
      return To<CSSAxisValue>(this)->CustomCSSText();
    case kBasicShapeCircleClass:
      return To<CSSBasicShapeCircleValue>(this)->CustomCSSText();
    case kBasicShapeEllipseClass:
      return To<CSSBasicShapeEllipseValue>(this)->CustomCSSText();
    case kBasicShapePolygonClass:
      return To<CSSBasicShapePolygonValue>(this)->CustomCSSText();
    case kBasicShapeInsetClass:
      return To<CSSBasicShapeInsetValue>(this)->CustomCSSText();
    case kBorderImageSliceClass:
      return To<CSSBorderImageSliceValue>(this)->CustomCSSText();
    case kColorClass:
      return To<CSSColorValue>(this)->CustomCSSText();
    case kCounterClass:
      return To<CSSCounterValue>(this)->CustomCSSText();
    case kCursorImageClass:
      return To<CSSCursorImageValue>(this)->CustomCSSText();
    case kFontFaceSrcClass:
      return To<CSSFontFaceSrcValue>(this)->CustomCSSText();
    case kFontFamilyClass:
      return To<CSSFontFamilyValue>(this)->CustomCSSText();
    case kFontFeatureClass:
      return To<CSSFontFeatureValue>(this)->CustomCSSText();
    case kFontStyleRangeClass:
      return To<CSSFontStyleRangeValue>(this)->CustomCSSText();
    case kFontVariationClass:
      return To<CSSFontVariationValue>(this)->CustomCSSText();
    case kFunctionClass:
      return To<CSSFunctionValue>(this)->CustomCSSText();
    case kLayoutFunctionClass:
      return To<CSSLayoutFunctionValue>(this)->CustomCSSText();
    case kLinearGradientClass:
      return To<CSSLinearGradientValue>(this)->CustomCSSText();
    case kRadialGradientClass:
      return To<CSSRadialGradientValue>(this)->CustomCSSText();
    case kConicGradientClass:
      return To<CSSConicGradientValue>(this)->CustomCSSText();
    case kCrossfadeClass:
      return To<CSSCrossfadeValue>(this)->CustomCSSText();
    case kPaintClass:
      return To<CSSPaintValue>(this)->CustomCSSText();
    case kCustomIdentClass:
      return To<CSSCustomIdentValue>(this)->CustomCSSText();
    case kImageClass:
      return To<CSSImageValue>(this)->CustomCSSText();
    case kInheritedClass:
      return To<CSSInheritedValue>(this)->CustomCSSText();
    case kUnsetClass:
      return To<CSSUnsetValue>(this)->CustomCSSText();
    case kInitialClass:
      return To<CSSInitialValue>(this)->CustomCSSText();
    case kGridAutoRepeatClass:
      return To<CSSGridAutoRepeatValue>(this)->CustomCSSText();
    case kGridIntegerRepeatClass:
      return To<CSSGridIntegerRepeatValue>(this)->CustomCSSText();
    case kGridLineNamesClass:
      return To<CSSGridLineNamesValue>(this)->CustomCSSText();
    case kGridTemplateAreasClass:
      return To<CSSGridTemplateAreasValue>(this)->CustomCSSText();
    case kPathClass:
      return To<CSSPathValue>(this)->CustomCSSText();
    case kNumericLiteralClass:
      return To<CSSNumericLiteralValue>(this)->CustomCSSText();
    case kMathFunctionClass:
      return To<CSSMathFunctionValue>(this)->CustomCSSText();
    case kRayClass:
      return To<CSSRayValue>(this)->CustomCSSText();
    case kIdentifierClass:
      return To<CSSIdentifierValue>(this)->CustomCSSText();
    case kKeyframeShorthandClass:
      return To<CSSKeyframeShorthandValue>(this)->CustomCSSText();
    case kQuadClass:
      return To<CSSQuadValue>(this)->CustomCSSText();
    case kReflectClass:
      return To<CSSReflectValue>(this)->CustomCSSText();
    case kShadowClass:
      return To<CSSShadowValue>(this)->CustomCSSText();
    case kStringClass:
      return To<CSSStringValue>(this)->CustomCSSText();
    case kCubicBezierTimingFunctionClass:
      return To<CSSCubicBezierTimingFunctionValue>(this)->CustomCSSText();
    case kStepsTimingFunctionClass:
      return To<CSSStepsTimingFunctionValue>(this)->CustomCSSText();
    case kUnicodeRangeClass:
      return To<CSSUnicodeRangeValue>(this)->CustomCSSText();
    case kURIClass:
      return To<CSSURIValue>(this)->CustomCSSText();
    case kValuePairClass:
      return To<CSSValuePair>(this)->CustomCSSText();
    case kValueListClass:
      return To<CSSValueList>(this)->CustomCSSText();
    case kImageSetClass:
      return To<CSSImageSetValue>(this)->CustomCSSText();
    case kCSSContentDistributionClass:
      return To<CSSContentDistributionValue>(this)->CustomCSSText();
    case kPendingInterpolationClass:
      return To<CSSPendingInterpolationValue>(this)->CustomCSSText();
    case kVariableReferenceClass:
      return To<CSSVariableReferenceValue>(this)->CustomCSSText();
    case kCustomPropertyDeclarationClass:
      return To<CSSCustomPropertyDeclaration>(this)->CustomCSSText();
    case kPendingSubstitutionValueClass:
      return To<CSSPendingSubstitutionValue>(this)->CustomCSSText();
    case kInvalidVariableValueClass:
      return To<CSSInvalidVariableValue>(this)->CustomCSSText();
  }
  NOTREACHED();
  return String();
}

void CSSValue::FinalizeGarbageCollectedObject() {
  switch (GetClassType()) {
    case kAxisClass:
      To<CSSAxisValue>(this)->~CSSAxisValue();
      return;
    case kBasicShapeCircleClass:
      To<CSSBasicShapeCircleValue>(this)->~CSSBasicShapeCircleValue();
      return;
    case kBasicShapeEllipseClass:
      To<CSSBasicShapeEllipseValue>(this)->~CSSBasicShapeEllipseValue();
      return;
    case kBasicShapePolygonClass:
      To<CSSBasicShapePolygonValue>(this)->~CSSBasicShapePolygonValue();
      return;
    case kBasicShapeInsetClass:
      To<CSSBasicShapeInsetValue>(this)->~CSSBasicShapeInsetValue();
      return;
    case kBorderImageSliceClass:
      To<CSSBorderImageSliceValue>(this)->~CSSBorderImageSliceValue();
      return;
    case kColorClass:
      To<CSSColorValue>(this)->~CSSColorValue();
      return;
    case kCounterClass:
      To<CSSCounterValue>(this)->~CSSCounterValue();
      return;
    case kCursorImageClass:
      To<CSSCursorImageValue>(this)->~CSSCursorImageValue();
      return;
    case kFontFaceSrcClass:
      To<CSSFontFaceSrcValue>(this)->~CSSFontFaceSrcValue();
      return;
    case kFontFamilyClass:
      To<CSSFontFamilyValue>(this)->~CSSFontFamilyValue();
      return;
    case kFontFeatureClass:
      To<CSSFontFeatureValue>(this)->~CSSFontFeatureValue();
      return;
    case kFontStyleRangeClass:
      To<CSSFontStyleRangeValue>(this)->~CSSFontStyleRangeValue();
      return;
    case kFontVariationClass:
      To<CSSFontVariationValue>(this)->~CSSFontVariationValue();
      return;
    case kFunctionClass:
      To<CSSFunctionValue>(this)->~CSSFunctionValue();
      return;
    case kLayoutFunctionClass:
      To<CSSLayoutFunctionValue>(this)->~CSSLayoutFunctionValue();
      return;
    case kLinearGradientClass:
      To<CSSLinearGradientValue>(this)->~CSSLinearGradientValue();
      return;
    case kRadialGradientClass:
      To<CSSRadialGradientValue>(this)->~CSSRadialGradientValue();
      return;
    case kConicGradientClass:
      To<CSSConicGradientValue>(this)->~CSSConicGradientValue();
      return;
    case kCrossfadeClass:
      To<CSSCrossfadeValue>(this)->~CSSCrossfadeValue();
      return;
    case kPaintClass:
      To<CSSPaintValue>(this)->~CSSPaintValue();
      return;
    case kCustomIdentClass:
      To<CSSCustomIdentValue>(this)->~CSSCustomIdentValue();
      return;
    case kImageClass:
      To<CSSImageValue>(this)->~CSSImageValue();
      return;
    case kInheritedClass:
      To<CSSInheritedValue>(this)->~CSSInheritedValue();
      return;
    case kInitialClass:
      To<CSSInitialValue>(this)->~CSSInitialValue();
      return;
    case kUnsetClass:
      To<CSSUnsetValue>(this)->~CSSUnsetValue();
      return;
    case kGridAutoRepeatClass:
      To<CSSGridAutoRepeatValue>(this)->~CSSGridAutoRepeatValue();
      return;
    case kGridIntegerRepeatClass:
      To<CSSGridIntegerRepeatValue>(this)->~CSSGridIntegerRepeatValue();
      return;
    case kGridLineNamesClass:
      To<CSSGridLineNamesValue>(this)->~CSSGridLineNamesValue();
      return;
    case kGridTemplateAreasClass:
      To<CSSGridTemplateAreasValue>(this)->~CSSGridTemplateAreasValue();
      return;
    case kPathClass:
      To<CSSPathValue>(this)->~CSSPathValue();
      return;
    case kNumericLiteralClass:
      To<CSSNumericLiteralValue>(this)->~CSSNumericLiteralValue();
      return;
    case kMathFunctionClass:
      To<CSSMathFunctionValue>(this)->~CSSMathFunctionValue();
      return;
    case kRayClass:
      To<CSSRayValue>(this)->~CSSRayValue();
      return;
    case kIdentifierClass:
      To<CSSIdentifierValue>(this)->~CSSIdentifierValue();
      return;
    case kKeyframeShorthandClass:
      To<CSSKeyframeShorthandValue>(this)->~CSSKeyframeShorthandValue();
      return;
    case kQuadClass:
      To<CSSQuadValue>(this)->~CSSQuadValue();
      return;
    case kReflectClass:
      To<CSSReflectValue>(this)->~CSSReflectValue();
      return;
    case kShadowClass:
      To<CSSShadowValue>(this)->~CSSShadowValue();
      return;
    case kStringClass:
      To<CSSStringValue>(this)->~CSSStringValue();
      return;
    case kCubicBezierTimingFunctionClass:
      To<CSSCubicBezierTimingFunctionValue>(this)
          ->~CSSCubicBezierTimingFunctionValue();
      return;
    case kStepsTimingFunctionClass:
      To<CSSStepsTimingFunctionValue>(this)->~CSSStepsTimingFunctionValue();
      return;
    case kUnicodeRangeClass:
      To<CSSUnicodeRangeValue>(this)->~CSSUnicodeRangeValue();
      return;
    case kURIClass:
      To<CSSURIValue>(this)->~CSSURIValue();
      return;
    case kValueListClass:
      To<CSSValueList>(this)->~CSSValueList();
      return;
    case kValuePairClass:
      To<CSSValuePair>(this)->~CSSValuePair();
      return;
    case kImageSetClass:
      To<CSSImageSetValue>(this)->~CSSImageSetValue();
      return;
    case kCSSContentDistributionClass:
      To<CSSContentDistributionValue>(this)->~CSSContentDistributionValue();
      return;
    case kPendingInterpolationClass:
      To<CSSPendingInterpolationValue>(this)->~CSSPendingInterpolationValue();
      return;
    case kVariableReferenceClass:
      To<CSSVariableReferenceValue>(this)->~CSSVariableReferenceValue();
      return;
    case kCustomPropertyDeclarationClass:
      To<CSSCustomPropertyDeclaration>(this)->~CSSCustomPropertyDeclaration();
      return;
    case kPendingSubstitutionValueClass:
      To<CSSPendingSubstitutionValue>(this)->~CSSPendingSubstitutionValue();
      return;
    case kInvalidVariableValueClass:
      To<CSSInvalidVariableValue>(this)->~CSSInvalidVariableValue();
      return;
  }
  NOTREACHED();
}

void CSSValue::Trace(blink::Visitor* visitor) {
  switch (GetClassType()) {
    case kAxisClass:
      To<CSSAxisValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kBasicShapeCircleClass:
      To<CSSBasicShapeCircleValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kBasicShapeEllipseClass:
      To<CSSBasicShapeEllipseValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kBasicShapePolygonClass:
      To<CSSBasicShapePolygonValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kBasicShapeInsetClass:
      To<CSSBasicShapeInsetValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kBorderImageSliceClass:
      To<CSSBorderImageSliceValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kColorClass:
      To<CSSColorValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kCounterClass:
      To<CSSCounterValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kCursorImageClass:
      To<CSSCursorImageValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kFontFaceSrcClass:
      To<CSSFontFaceSrcValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kFontFamilyClass:
      To<CSSFontFamilyValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kFontFeatureClass:
      To<CSSFontFeatureValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kFontStyleRangeClass:
      To<CSSFontStyleRangeValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kFontVariationClass:
      To<CSSFontVariationValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kFunctionClass:
      To<CSSFunctionValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kLayoutFunctionClass:
      To<CSSLayoutFunctionValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kLinearGradientClass:
      To<CSSLinearGradientValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kRadialGradientClass:
      To<CSSRadialGradientValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kConicGradientClass:
      To<CSSConicGradientValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kCrossfadeClass:
      To<CSSCrossfadeValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kPaintClass:
      To<CSSPaintValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kCustomIdentClass:
      To<CSSCustomIdentValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kImageClass:
      To<CSSImageValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kInheritedClass:
      To<CSSInheritedValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kInitialClass:
      To<CSSInitialValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kUnsetClass:
      To<CSSUnsetValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kGridAutoRepeatClass:
      To<CSSGridAutoRepeatValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kGridIntegerRepeatClass:
      To<CSSGridIntegerRepeatValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kGridLineNamesClass:
      To<CSSGridLineNamesValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kGridTemplateAreasClass:
      To<CSSGridTemplateAreasValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kPathClass:
      To<CSSPathValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kNumericLiteralClass:
      To<CSSNumericLiteralValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kMathFunctionClass:
      To<CSSMathFunctionValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kRayClass:
      To<CSSRayValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kIdentifierClass:
      To<CSSIdentifierValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kKeyframeShorthandClass:
      To<CSSKeyframeShorthandValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kQuadClass:
      To<CSSQuadValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kReflectClass:
      To<CSSReflectValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kShadowClass:
      To<CSSShadowValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kStringClass:
      To<CSSStringValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kCubicBezierTimingFunctionClass:
      To<CSSCubicBezierTimingFunctionValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kStepsTimingFunctionClass:
      To<CSSStepsTimingFunctionValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kUnicodeRangeClass:
      To<CSSUnicodeRangeValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kURIClass:
      To<CSSURIValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kValueListClass:
      To<CSSValueList>(this)->TraceAfterDispatch(visitor);
      return;
    case kValuePairClass:
      To<CSSValuePair>(this)->TraceAfterDispatch(visitor);
      return;
    case kImageSetClass:
      To<CSSImageSetValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kCSSContentDistributionClass:
      To<CSSContentDistributionValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kPendingInterpolationClass:
      To<CSSPendingInterpolationValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kVariableReferenceClass:
      To<CSSVariableReferenceValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kCustomPropertyDeclarationClass:
      To<CSSCustomPropertyDeclaration>(this)->TraceAfterDispatch(visitor);
      return;
    case kPendingSubstitutionValueClass:
      To<CSSPendingSubstitutionValue>(this)->TraceAfterDispatch(visitor);
      return;
    case kInvalidVariableValueClass:
      To<CSSInvalidVariableValue>(this)->TraceAfterDispatch(visitor);
      return;
  }
  NOTREACHED();
}

}  // namespace blink
