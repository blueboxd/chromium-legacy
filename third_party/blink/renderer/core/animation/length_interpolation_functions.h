// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/991672): Rename to |interpolable_length.h|

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_LENGTH_INTERPOLATION_FUNCTIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_LENGTH_INTERPOLATION_FUNCTIONS_H_

#include <memory>
#include "third_party/blink/renderer/core/animation/interpolation_value.h"
#include "third_party/blink/renderer/core/animation/pairwise_interpolation_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class CSSToLengthConversionData;
class UnderlyingValue;

class CORE_EXPORT InterpolableLength final : public InterpolableValue {
 public:
  ~InterpolableLength() final {}
  InterpolableLength(const CSSLengthArray& length_array)
      : length_array_(length_array) {}
  InterpolableLength(CSSLengthArray&& length_array)
      : length_array_(std::move(length_array)) {}

  static std::unique_ptr<InterpolableLength> CreatePixels(double pixels);
  static std::unique_ptr<InterpolableLength> CreatePercent(double pixels);
  static std::unique_ptr<InterpolableLength> CreateNeutral();

  static std::unique_ptr<InterpolableLength> MaybeConvertCSSValue(
      const CSSValue& value);
  static std::unique_ptr<InterpolableLength> MaybeConvertLength(
      const Length& length,
      float zoom);

  static PairwiseInterpolationValue MergeSingles(
      std::unique_ptr<InterpolableValue> start,
      std::unique_ptr<InterpolableValue> end);

  Length CreateLength(const CSSToLengthConversionData& conversion_data,
                      ValueRange range) const;
  const CSSPrimitiveValue* CreateCSSValue(ValueRange range) const;

  bool HasPercentage() const {
    return length_array_.type_flags.test(
        CSSPrimitiveValue::kUnitTypePercentage);
  }
  void SubtractFromOneHundredPercent();

  // InterpolableValue:
  bool IsLength() const final { return true; }
  bool Equals(const InterpolableValue& other) const final {
    NOTREACHED();
    return false;
  }
  std::unique_ptr<InterpolableValue> Clone() const final {
    return std::make_unique<InterpolableLength>(length_array_);
  }
  std::unique_ptr<InterpolableValue> CloneAndZero() const final {
    return std::make_unique<InterpolableLength>(CSSLengthArray());
  }
  void Scale(double scale) final {
    for (double& value : length_array_.values) {
      value *= scale;
    }
  }
  void ScaleAndAdd(double scale, const InterpolableValue& other) final;
  void AssertCanInterpolateWith(const InterpolableValue& other) const final;

 private:
  // InterpolableValue:
  void Interpolate(const InterpolableValue& to,
                   const double progress,
                   InterpolableValue& result) const final;

  CSSLengthArray length_array_;
};

template <>
struct DowncastTraits<InterpolableLength> {
  static bool AllowFrom(const InterpolableValue& interpolable_value) {
    return interpolable_value.IsLength();
  }
};

// TODO(crbug.com/991672): Remove this trivial wrapper class.
class LengthInterpolationFunctions {
  STATIC_ONLY(LengthInterpolationFunctions);

 public:
  static std::unique_ptr<InterpolableValue> CreateInterpolablePixels(
      double pixels);
  static std::unique_ptr<InterpolableValue> CreateInterpolablePercent(
      double percent);
  static std::unique_ptr<InterpolableValue> CreateNeutralInterpolableValue();

  static InterpolationValue MaybeConvertCSSValue(const CSSValue&);
  static InterpolationValue MaybeConvertLength(const Length&, float zoom);
  static PairwiseInterpolationValue MergeSingles(InterpolationValue&& start,
                                                 InterpolationValue&& end);
  static bool NonInterpolableValuesAreCompatible(const NonInterpolableValue*,
                                                 const NonInterpolableValue*);
  static bool HasPercentage(const InterpolableValue&);
  static void Composite(UnderlyingValue&,
                        double underlying_fraction,
                        const InterpolableValue&,
                        const NonInterpolableValue*);
  static Length CreateLength(const InterpolableValue&,
                             const NonInterpolableValue*,
                             const CSSToLengthConversionData&,
                             ValueRange);

  // Unlike createLength() this preserves all specificed unit types via calc()
  // expressions.
  static const CSSValue* CreateCSSValue(const InterpolableValue&,
                                        const NonInterpolableValue*,
                                        ValueRange);

  static void SubtractFromOneHundredPercent(InterpolableValue& result);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_LENGTH_INTERPOLATION_FUNCTIONS_H_
