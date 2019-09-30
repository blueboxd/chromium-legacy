// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_transform_interpolation_type.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/animation/length_units_checker.h"
#include "third_party/blink/renderer/core/css/css_function_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/css/resolver/transform_builder.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/transforms/transform_operations.h"
#include "third_party/blink/renderer/platform/transforms/translate_transform_operation.h"

namespace blink {

class CSSTransformNonInterpolableValue : public NonInterpolableValue {
 public:
  static scoped_refptr<CSSTransformNonInterpolableValue> Create(
      TransformOperations&& transform) {
    const bool is_single = true;
    const bool is_additive = false;
    return base::AdoptRef(new CSSTransformNonInterpolableValue(
        is_single, std::move(transform), EmptyTransformOperations(),
        is_additive, is_additive));
  }

  static scoped_refptr<CSSTransformNonInterpolableValue> CreateAdditive(
      const CSSTransformNonInterpolableValue& other) {
    DCHECK(other.is_single_);
    const bool is_single = true;
    const bool is_additive = true;
    return base::AdoptRef(new CSSTransformNonInterpolableValue(
        is_single, TransformOperations(other.start_),
        TransformOperations(other.end_), is_additive, is_additive));
  }

  static scoped_refptr<CSSTransformNonInterpolableValue> Create(
      const CSSTransformNonInterpolableValue& start,
      double start_fraction,
      const CSSTransformNonInterpolableValue& end,
      double end_fraction) {
    return base::AdoptRef(new CSSTransformNonInterpolableValue(
        false, start.GetInterpolatedTransform(start_fraction),
        end.GetInterpolatedTransform(end_fraction), start.IsAdditive(),
        end.IsAdditive()));
  }

  scoped_refptr<CSSTransformNonInterpolableValue> Composite(
      const CSSTransformNonInterpolableValue& other,
      double other_progress) const {
    DCHECK(is_single_);
    DCHECK(!IsAdditive());

    // This is the case where we have no B, so the equation is U + A.
    if (other.is_single_) {
      DCHECK_EQ(other_progress, 0);
      DCHECK(other.IsAdditive());
      TransformOperations result;
      result.Operations() = Concat(Transform(), other.Transform());
      return Create(std::move(result));
    }

    // Otherwise, we must compute (U + A)(1 - f) + (U + B)f - where U is only
    // included if the keyframe is additive. This requires pre-pending the
    // underlying ops to the necessary sides and then performing the
    // interpolation.
    DCHECK(other.is_start_additive_ || other.is_end_additive_);
    TransformOperations start;
    start.Operations() = other.is_start_additive_
                             ? Concat(Transform(), other.start_)
                             : other.start_.Operations();
    TransformOperations end;
    end.Operations() = other.is_end_additive_ ? Concat(Transform(), other.end_)
                                              : other.end_.Operations();
    return Create(end.Blend(start, other_progress));
  }

  TransformOperations GetInterpolatedTransform(double progress) const {
    if (progress == 0)
      return start_;
    if (progress == 1)
      return end_;
    DCHECK(!IsAdditive());
    return end_.Blend(start_, progress);
  }

  bool IsSingle() const { return is_single_; }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  CSSTransformNonInterpolableValue(bool is_single,
                                   TransformOperations&& start,
                                   TransformOperations&& end,
                                   bool is_start_additive,
                                   bool is_end_additive)
      : is_single_(is_single),
        start_(std::move(start)),
        end_(std::move(end)),
        is_start_additive_(is_start_additive),
        is_end_additive_(is_end_additive) {}

  const TransformOperations& Transform() const {
    DCHECK(is_single_);
    return start_;
  }
  bool IsAdditive() const {
    bool result = is_start_additive_ || is_end_additive_;
    DCHECK(!result || is_single_);
    return result;
  }

  Vector<scoped_refptr<TransformOperation>> Concat(
      const TransformOperations& a,
      const TransformOperations& b) const {
    Vector<scoped_refptr<TransformOperation>> result;
    result.ReserveCapacity(a.size() + b.size());
    result.AppendVector(a.Operations());
    result.AppendVector(b.Operations());
    return result;
  }

  bool is_single_;
  TransformOperations start_;
  TransformOperations end_;
  bool is_start_additive_;
  bool is_end_additive_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSTransformNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(CSSTransformNonInterpolableValue);

namespace {

InterpolationValue ConvertTransform(TransformOperations&& transform) {
  return InterpolationValue(
      std::make_unique<InterpolableNumber>(0),
      CSSTransformNonInterpolableValue::Create(std::move(transform)));
}

InterpolationValue ConvertTransform(const TransformOperations& transform) {
  return ConvertTransform(TransformOperations(transform));
}

class InheritedTransformChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  InheritedTransformChecker(const TransformOperations& inherited_transform)
      : inherited_transform_(inherited_transform) {}

  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    return inherited_transform_ == state.ParentStyle()->Transform();
  }

 private:
  const TransformOperations inherited_transform_;
};

// Performs interpolation for the UnderlyingValueOwner, if necessary. This
// requires us to:
//
//   i. Compute the interpolation for the CSSTransformNonInterpolableValue.
//   ii. Reset the underlying_value_owner's interpolable_value (which is the
//       progress) to 0. This is necessary to avoid double-interpolating in
//       ApplyStandardPropertyValue.
void InterpolateUnderlyingValueOwnerIfNecessary(
    UnderlyingValueOwner& underlying_value_owner) {
  const CSSTransformNonInterpolableValue& underlying_non_interpolable_value =
      ToCSSTransformNonInterpolableValue(
          *underlying_value_owner.Value().non_interpolable_value);
  // If the UnderlyingValueOwner is already single, it is either based on the
  // underlying CSS style itself, or has already been interpolated.
  if (underlying_non_interpolable_value.IsSingle())
    return;

  double underlying_progress =
      ToInterpolableNumber(*underlying_value_owner.Value().interpolable_value)
          .Value();
  underlying_value_owner.SetInterpolableValue(
      std::make_unique<InterpolableNumber>(0));
  underlying_value_owner.SetNonInterpolableValue(
      CSSTransformNonInterpolableValue::Create(
          underlying_non_interpolable_value.GetInterpolatedTransform(
              underlying_progress)));
}

}  // namespace

InterpolationValue CSSTransformInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers&) const {
  return ConvertTransform(EmptyTransformOperations());
}

InterpolationValue CSSTransformInterpolationType::MaybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers&) const {
  return ConvertTransform(ComputedStyle::InitialStyle().Transform());
}

InterpolationValue CSSTransformInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  const TransformOperations& inherited_transform =
      state.ParentStyle()->Transform();
  conversion_checkers.push_back(
      std::make_unique<InheritedTransformChecker>(inherited_transform));
  return ConvertTransform(inherited_transform);
}

InterpolationValue CSSTransformInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState* state,
    ConversionCheckers& conversion_checkers) const {
  DCHECK(state);
  if (auto* list_value = DynamicTo<CSSValueList>(value)) {
    CSSPrimitiveValue::LengthTypeFlags types;
    for (const CSSValue* item : *list_value) {
      const auto& transform_function = To<CSSFunctionValue>(*item);
      if (transform_function.FunctionType() == CSSValueID::kMatrix ||
          transform_function.FunctionType() == CSSValueID::kMatrix3d) {
        types.set(CSSPrimitiveValue::kUnitTypePixels);
        continue;
      }
      for (const CSSValue* argument : transform_function) {
        const auto& primitive_value = To<CSSPrimitiveValue>(*argument);
        if (!primitive_value.IsLength())
          continue;
        primitive_value.AccumulateLengthUnitTypes(types);
      }
    }
    std::unique_ptr<InterpolationType::ConversionChecker> length_units_checker =
        LengthUnitsChecker::MaybeCreate(types, *state);

    if (length_units_checker)
      conversion_checkers.push_back(std::move(length_units_checker));
  }

  DCHECK(state);
  TransformOperations transform = TransformBuilder::CreateTransformOperations(
      value, state->CssToLengthConversionData());
  return ConvertTransform(std::move(transform));
}

InterpolationValue CSSTransformInterpolationType::MakeAdditive(
    InterpolationValue value,
    const InterpolationValue& underlying) const {
  value.non_interpolable_value =
      CSSTransformNonInterpolableValue::CreateAdditive(
          ToCSSTransformNonInterpolableValue(*value.non_interpolable_value));
  return value;
}

PairwiseInterpolationValue CSSTransformInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  double start_fraction =
      ToInterpolableNumber(*start.interpolable_value).Value();
  double end_fraction = ToInterpolableNumber(*end.interpolable_value).Value();
  return PairwiseInterpolationValue(
      std::make_unique<InterpolableNumber>(0),
      std::make_unique<InterpolableNumber>(1),
      CSSTransformNonInterpolableValue::Create(
          std::move(ToCSSTransformNonInterpolableValue(
              *start.non_interpolable_value)),
          start_fraction,
          std::move(
              ToCSSTransformNonInterpolableValue(*end.non_interpolable_value)),
          end_fraction));
}

InterpolationValue
CSSTransformInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  return ConvertTransform(style.Transform());
}

void CSSTransformInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  // If the first InvalidatableInterpolation in the stack doesn't depend on an
  // underlying value, it becomes the underlying value, U. However at this point
  // U has not yet been interpolated (as interpolation for
  // CSSTransformInterpolationType only happens in either Composite or
  // ApplyStandardPropertyValue), and so we have to do it here.
  InterpolateUnderlyingValueOwnerIfNecessary(underlying_value_owner);

  // Now that U has been resolved, do the actual compositing.
  const CSSTransformNonInterpolableValue& underlying_non_interpolable_value =
      ToCSSTransformNonInterpolableValue(
          *underlying_value_owner.GetNonInterpolableValue());
  const CSSTransformNonInterpolableValue& non_interpolable_value =
      ToCSSTransformNonInterpolableValue(*value.non_interpolable_value);
  double progress = ToInterpolableNumber(*value.interpolable_value).Value();
  underlying_value_owner.SetNonInterpolableValue(
      underlying_non_interpolable_value.Composite(non_interpolable_value,
                                                  progress));
}

void CSSTransformInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* untyped_non_interpolable_value,
    StyleResolverState& state) const {
  double progress = ToInterpolableNumber(interpolable_value).Value();
  const CSSTransformNonInterpolableValue& non_interpolable_value =
      ToCSSTransformNonInterpolableValue(*untyped_non_interpolable_value);
  state.Style()->SetTransform(
      non_interpolable_value.GetInterpolatedTransform(progress));
}

}  // namespace blink
