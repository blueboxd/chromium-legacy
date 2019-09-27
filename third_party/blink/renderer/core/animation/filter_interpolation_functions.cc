// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/filter_interpolation_functions.h"

#include <memory>
#include "third_party/blink/renderer/core/animation/interpolable_length.h"
#include "third_party/blink/renderer/core/animation/interpolable_shadow.h"
#include "third_party/blink/renderer/core/css/css_function_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/resolver/filter_operation_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/filter_operations.h"
#include "third_party/blink/renderer/core/style/shadow_data.h"

namespace blink {

class FilterNonInterpolableValue : public NonInterpolableValue {
 public:
  static scoped_refptr<FilterNonInterpolableValue> Create(
      FilterOperation::OperationType type,
      scoped_refptr<const NonInterpolableValue> type_non_interpolable_value) {
    return base::AdoptRef(new FilterNonInterpolableValue(
        type, std::move(type_non_interpolable_value)));
  }

  FilterOperation::OperationType GetOperationType() const { return type_; }
  const NonInterpolableValue* TypeNonInterpolableValue() const {
    return type_non_interpolable_value_.get();
  }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  FilterNonInterpolableValue(
      FilterOperation::OperationType type,
      scoped_refptr<const NonInterpolableValue> type_non_interpolable_value)
      : type_(type),
        type_non_interpolable_value_(std::move(type_non_interpolable_value)) {}

  const FilterOperation::OperationType type_;
  scoped_refptr<const NonInterpolableValue> type_non_interpolable_value_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(FilterNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(FilterNonInterpolableValue);

namespace {

double ClampParameter(double value, FilterOperation::OperationType type) {
  switch (type) {
    case FilterOperation::BRIGHTNESS:
    case FilterOperation::CONTRAST:
    case FilterOperation::SATURATE:
      return clampTo<double>(value, 0);

    case FilterOperation::GRAYSCALE:
    case FilterOperation::INVERT:
    case FilterOperation::OPACITY:
    case FilterOperation::SEPIA:
      return clampTo<double>(value, 0, 1);

    case FilterOperation::HUE_ROTATE:
      return value;

    default:
      NOTREACHED();
      return 0;
  }
}

}  // namespace

InterpolationValue filter_interpolation_functions::MaybeConvertCSSFilter(
    const CSSValue& value) {
  if (value.IsURIValue())
    return nullptr;

  const auto& filter = To<CSSFunctionValue>(value);
  DCHECK_LE(filter.length(), 1u);
  FilterOperation::OperationType type =
      FilterOperationResolver::FilterOperationForType(filter.FunctionType());
  InterpolationValue result = nullptr;

  switch (type) {
    case FilterOperation::BRIGHTNESS:
    case FilterOperation::CONTRAST:
    case FilterOperation::GRAYSCALE:
    case FilterOperation::INVERT:
    case FilterOperation::OPACITY:
    case FilterOperation::SATURATE:
    case FilterOperation::SEPIA:
    case FilterOperation::HUE_ROTATE:
      result.interpolable_value = std::make_unique<InterpolableNumber>(
          FilterOperationResolver::ResolveNumericArgumentForFunction(filter));
      break;

    case FilterOperation::BLUR: {
      if (filter.length() == 0) {
        result.interpolable_value = InterpolableLength::CreateNeutral();
      } else {
        result = InterpolationValue(
            InterpolableLength::MaybeConvertCSSValue(filter.Item(0)));
      }
      break;
    }

    case FilterOperation::DROP_SHADOW:
      result.interpolable_value =
          InterpolableShadow::MaybeConvertCSSValue(filter.Item(0));
      break;

    default:
      NOTREACHED();
      return nullptr;
  }

  if (!result)
    return nullptr;

  result.non_interpolable_value = FilterNonInterpolableValue::Create(
      type, std::move(result.non_interpolable_value));
  return result;
}

InterpolationValue filter_interpolation_functions::MaybeConvertFilter(
    const FilterOperation& filter,
    double zoom) {
  InterpolationValue result = nullptr;

  switch (filter.GetType()) {
    case FilterOperation::GRAYSCALE:
    case FilterOperation::HUE_ROTATE:
    case FilterOperation::SATURATE:
    case FilterOperation::SEPIA:
      result.interpolable_value = std::make_unique<InterpolableNumber>(
          To<BasicColorMatrixFilterOperation>(filter).Amount());
      break;

    case FilterOperation::BRIGHTNESS:
    case FilterOperation::CONTRAST:
    case FilterOperation::INVERT:
    case FilterOperation::OPACITY:
      result.interpolable_value = std::make_unique<InterpolableNumber>(
          To<BasicComponentTransferFilterOperation>(filter).Amount());
      break;

    case FilterOperation::BLUR:
      result = InterpolationValue(InterpolableLength::MaybeConvertLength(
          To<BlurFilterOperation>(filter).StdDeviation(), zoom));
      break;

    case FilterOperation::DROP_SHADOW:
      result.interpolable_value = InterpolableShadow::Create(
          To<DropShadowFilterOperation>(filter).Shadow(), zoom);
      break;

    case FilterOperation::REFERENCE:
      return nullptr;

    default:
      NOTREACHED();
      return nullptr;
  }

  if (!result)
    return nullptr;

  result.non_interpolable_value = FilterNonInterpolableValue::Create(
      filter.GetType(), std::move(result.non_interpolable_value));
  return result;
}

std::unique_ptr<InterpolableValue>
filter_interpolation_functions::CreateNoneValue(
    const NonInterpolableValue& untyped_value) {
  switch (ToFilterNonInterpolableValue(untyped_value).GetOperationType()) {
    case FilterOperation::GRAYSCALE:
    case FilterOperation::INVERT:
    case FilterOperation::SEPIA:
    case FilterOperation::HUE_ROTATE:
      return std::make_unique<InterpolableNumber>(0);

    case FilterOperation::BRIGHTNESS:
    case FilterOperation::CONTRAST:
    case FilterOperation::OPACITY:
    case FilterOperation::SATURATE:
      return std::make_unique<InterpolableNumber>(1);

    case FilterOperation::BLUR:
      return InterpolableLength::CreateNeutral();

    case FilterOperation::DROP_SHADOW:
      return InterpolableShadow::CreateNeutral();

    default:
      NOTREACHED();
      return nullptr;
  }
}

bool filter_interpolation_functions::FiltersAreCompatible(
    const NonInterpolableValue& a,
    const NonInterpolableValue& b) {
  return ToFilterNonInterpolableValue(a).GetOperationType() ==
         ToFilterNonInterpolableValue(b).GetOperationType();
}

FilterOperation* filter_interpolation_functions::CreateFilter(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue& untyped_non_interpolable_value,
    const StyleResolverState& state) {
  const FilterNonInterpolableValue& non_interpolable_value =
      ToFilterNonInterpolableValue(untyped_non_interpolable_value);
  FilterOperation::OperationType type =
      non_interpolable_value.GetOperationType();

  switch (type) {
    case FilterOperation::GRAYSCALE:
    case FilterOperation::HUE_ROTATE:
    case FilterOperation::SATURATE:
    case FilterOperation::SEPIA: {
      double value = ClampParameter(
          ToInterpolableNumber(interpolable_value).Value(), type);
      return MakeGarbageCollected<BasicColorMatrixFilterOperation>(value, type);
    }

    case FilterOperation::BRIGHTNESS:
    case FilterOperation::CONTRAST:
    case FilterOperation::INVERT:
    case FilterOperation::OPACITY: {
      double value = ClampParameter(
          ToInterpolableNumber(interpolable_value).Value(), type);
      return MakeGarbageCollected<BasicComponentTransferFilterOperation>(value,
                                                                         type);
    }

    case FilterOperation::BLUR: {
      Length std_deviation =
          To<InterpolableLength>(interpolable_value)
              .CreateLength(state.CssToLengthConversionData(),
                            kValueRangeNonNegative);
      return MakeGarbageCollected<BlurFilterOperation>(std_deviation);
    }

    case FilterOperation::DROP_SHADOW: {
      ShadowData shadow_data =
          To<InterpolableShadow>(interpolable_value).CreateShadowData(state);
      if (shadow_data.GetColor().IsCurrentColor())
        shadow_data.OverrideColor(Color::kBlack);
      return DropShadowFilterOperation::Create(shadow_data);
    }

    default:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace blink
