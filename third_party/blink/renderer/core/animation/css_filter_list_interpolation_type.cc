// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_filter_list_interpolation_type.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/animation/interpolable_filter.h"
#include "third_party/blink/renderer/core/animation/list_interpolation_functions.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

const FilterOperations& GetFilterList(const CSSProperty& property,
                                      const ComputedStyle& style) {
  switch (property.PropertyID()) {
    default:
      NOTREACHED();
      FALLTHROUGH;
    case CSSPropertyID::kBackdropFilter:
      return style.BackdropFilter();
    case CSSPropertyID::kFilter:
      return style.Filter();
  }
}

void SetFilterList(const CSSProperty& property,
                   ComputedStyle& style,
                   const FilterOperations& filter_operations) {
  switch (property.PropertyID()) {
    case CSSPropertyID::kBackdropFilter:
      style.SetBackdropFilter(filter_operations);
      break;
    case CSSPropertyID::kFilter:
      style.SetFilter(filter_operations);
      break;
    default:
      NOTREACHED();
      break;
  }
}

class UnderlyingFilterListChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  UnderlyingFilterListChecker(const InterpolableList* interpolable_list) {
    wtf_size_t length = interpolable_list->length();
    types_.ReserveInitialCapacity(length);
    for (wtf_size_t i = 0; i < length; i++) {
      types_.push_back(
          To<InterpolableFilter>(interpolable_list->Get(i))->GetType());
    }
  }

  bool IsValid(const StyleResolverState&,
               const InterpolationValue& underlying) const final {
    const InterpolableList& underlying_list =
        ToInterpolableList(*underlying.interpolable_value);
    if (underlying_list.length() != types_.size())
      return false;
    for (wtf_size_t i = 0; i < types_.size(); i++) {
      FilterOperation::OperationType other_type =
          To<InterpolableFilter>(underlying_list.Get(i))->GetType();
      if (types_[i] != other_type)
        return false;
    }
    return true;
  }

 private:
  Vector<FilterOperation::OperationType> types_;
};

class InheritedFilterListChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  InheritedFilterListChecker(const CSSProperty& property,
                             const FilterOperations& filter_operations)
      : property_(property),
        filter_operations_wrapper_(
            MakeGarbageCollected<FilterOperationsWrapper>(filter_operations)) {}

  bool IsValid(const StyleResolverState& state,
               const InterpolationValue&) const final {
    const FilterOperations& filter_operations =
        filter_operations_wrapper_->Operations();
    return filter_operations == GetFilterList(property_, *state.ParentStyle());
  }

 private:
  const CSSProperty& property_;
  Persistent<FilterOperationsWrapper> filter_operations_wrapper_;
};

InterpolationValue ConvertFilterList(const FilterOperations& filter_operations,
                                     double zoom) {
  wtf_size_t length = filter_operations.size();
  auto interpolable_list = std::make_unique<InterpolableList>(length);
  for (wtf_size_t i = 0; i < length; i++) {
    std::unique_ptr<InterpolableFilter> result =
        InterpolableFilter::MaybeCreate(*filter_operations.Operations()[i],
                                        zoom);
    if (!result)
      return nullptr;
    interpolable_list->Set(i, std::move(result));
  }
  return InterpolationValue(std::move(interpolable_list));
}

}  // namespace

InterpolationValue CSSFilterListInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  const InterpolableList* interpolable_list =
      ToInterpolableList(underlying.interpolable_value.get());
  conversion_checkers.push_back(
      std::make_unique<UnderlyingFilterListChecker>(interpolable_list));
  // The neutral value for composition for a filter list is the empty list, as
  // the additive operator is concatenation, so concat(underlying, []) ==
  // underlying.
  return InterpolationValue(std::make_unique<InterpolableList>(0));
}

InterpolationValue CSSFilterListInterpolationType::MaybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers& conversion_checkers) const {
  return ConvertFilterList(
      GetFilterList(CssProperty(), ComputedStyle::InitialStyle()), 1);
}

InterpolationValue CSSFilterListInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  const FilterOperations& inherited_filter_operations =
      GetFilterList(CssProperty(), *state.ParentStyle());
  conversion_checkers.push_back(std::make_unique<InheritedFilterListChecker>(
      CssProperty(), inherited_filter_operations));
  return ConvertFilterList(inherited_filter_operations,
                           state.Style()->EffectiveZoom());
}

InterpolationValue CSSFilterListInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (identifier_value && identifier_value->GetValueID() == CSSValueID::kNone)
    return InterpolationValue(std::make_unique<InterpolableList>(0));

  if (!value.IsBaseValueList())
    return nullptr;

  const auto& list = To<CSSValueList>(value);
  wtf_size_t length = list.length();
  auto interpolable_list = std::make_unique<InterpolableList>(length);
  for (wtf_size_t i = 0; i < length; i++) {
    std::unique_ptr<InterpolableFilter> result =
        InterpolableFilter::MaybeConvertCSSValue(list.Item(i));
    if (!result)
      return nullptr;
    interpolable_list->Set(i, std::move(result));
  }
  return InterpolationValue(std::move(interpolable_list));
}

InterpolationValue
CSSFilterListInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  return ConvertFilterList(GetFilterList(CssProperty(), style),
                           style.EffectiveZoom());
}

PairwiseInterpolationValue CSSFilterListInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  InterpolableList& start_interpolable_list =
      ToInterpolableList(*start.interpolable_value);
  InterpolableList& end_interpolable_list =
      ToInterpolableList(*end.interpolable_value);
  wtf_size_t start_length = start_interpolable_list.length();
  wtf_size_t end_length = end_interpolable_list.length();

  for (wtf_size_t i = 0; i < start_length && i < end_length; i++) {
    if (To<InterpolableFilter>(start_interpolable_list.Get(i))->GetType() !=
        To<InterpolableFilter>(end_interpolable_list.Get(i))->GetType())
      return nullptr;
  }

  if (start_length == end_length) {
    return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                      std::move(end.interpolable_value));
  }

  // Extend the shorter InterpolableList with neutral values that are compatible
  // with corresponding filters in the longer list.
  InterpolationValue& shorter = start_length < end_length ? start : end;
  wtf_size_t shorter_length = std::min(start_length, end_length);
  wtf_size_t longer_length = std::max(start_length, end_length);
  InterpolableList& shorter_interpolable_list = start_length < end_length
                                                    ? start_interpolable_list
                                                    : end_interpolable_list;
  const InterpolableList& longer_interpolable_list =
      start_length < end_length ? end_interpolable_list
                                : start_interpolable_list;
  auto extended_interpolable_list =
      std::make_unique<InterpolableList>(longer_length);
  for (wtf_size_t i = 0; i < longer_length; i++) {
    if (i < shorter_length)
      extended_interpolable_list->Set(
          i, std::move(shorter_interpolable_list.GetMutable(i)));
    else
      extended_interpolable_list->Set(
          i, InterpolableFilter::CreateInitialValue(
                 To<InterpolableFilter>(longer_interpolable_list.Get(i))
                     ->GetType()));
  }
  shorter.interpolable_value = std::move(extended_interpolable_list);

  return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                    std::move(end.interpolable_value));
}

void CSSFilterListInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  // We do our actual compositing behavior in |MakeAdditive|; see the
  // documentation on that method.
  underlying_value_owner.Set(*this, value);
}

void CSSFilterListInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  const InterpolableList& interpolable_list =
      ToInterpolableList(interpolable_value);
  wtf_size_t length = interpolable_list.length();

  FilterOperations filter_operations;
  filter_operations.Operations().ReserveCapacity(length);
  for (wtf_size_t i = 0; i < length; i++) {
    filter_operations.Operations().push_back(
        To<InterpolableFilter>(interpolable_list.Get(i))
            ->CreateFilterOperation(state));
  }
  SetFilterList(CssProperty(), *state.Style(), std::move(filter_operations));
}

InterpolationValue CSSFilterListInterpolationType::MakeAdditive(
    InterpolationValue value,
    const InterpolationValue& underlying) const {
  DCHECK(!value.non_interpolable_value);
  DCHECK(!underlying.non_interpolable_value);

  // By default, the interpolation stack attempts to optimize composition by
  // doing it after interpolation. This does not work in the case of filter
  // lists, as they have a composition behavior of concatenation. To work around
  // that, we hackily perform our composition in MakeAdditive (which runs before
  // interpolation), and then make Composite a simple replacement of the
  // underlying value (which we have already incorporated here).

  // The underlying value can be nullptr, most commonly if it contains a url().
  if (!underlying.interpolable_value)
    return nullptr;

  const InterpolableList& interpolable_list =
      ToInterpolableList(*value.interpolable_value);
  const InterpolableList& underlying_list =
      ToInterpolableList(*underlying.interpolable_value);

  auto composited_list = std::make_unique<InterpolableList>(
      underlying_list.length() + interpolable_list.length());
  for (wtf_size_t i = 0; i < composited_list->length(); i++) {
    if (i < underlying_list.length()) {
      composited_list->Set(i, underlying_list.Get(i)->Clone());
    } else {
      composited_list->Set(
          i, interpolable_list.Get(i - underlying_list.length())->Clone());
    }
  }

  return InterpolationValue(std::move(composited_list));
}

}  // namespace blink
