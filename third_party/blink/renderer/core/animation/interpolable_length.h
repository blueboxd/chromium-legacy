// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INTERPOLABLE_LENGTH_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INTERPOLABLE_LENGTH_H_

#include <memory>
#include "third_party/blink/renderer/core/animation/interpolation_value.h"
#include "third_party/blink/renderer/core/animation/pairwise_interpolation_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class CSSToLengthConversionData;
class CSSMathExpressionNode;
class CSSNumericLiteralValue;
class CSSValue;

class CORE_EXPORT InterpolableLength final : public InterpolableValue {
 public:
  ~InterpolableLength() final {}

  InterpolableLength();
  InterpolableLength(double value, CSSPrimitiveValue::UnitType);
  explicit InterpolableLength(const CSSNumericLiteralValue& value);
  explicit InterpolableLength(const CSSMathExpressionNode& expression);

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

  // Unlike CreateLength() this preserves all specified unit types via calc()
  // expressions.
  const CSSPrimitiveValue* CreateCSSValue(ValueRange range) const;

  // Mix the length with the percentage type, if it's not mixed already
  void SetHasPercentage();
  bool HasPercentage() const;
  void SubtractFromOneHundredPercent();

  // InterpolableValue:
  bool IsLength() const final { return true; }
  bool Equals(const InterpolableValue& other) const final {
    NOTREACHED();
    return false;
  }
  std::unique_ptr<InterpolableValue> Clone() const final;
  std::unique_ptr<InterpolableValue> CloneAndZero() const final {
    return std::make_unique<InterpolableLength>();
  }
  void Scale(double scale) final;
  void ScaleAndAdd(double scale, const InterpolableValue& other) final;
  void AssertCanInterpolateWith(const InterpolableValue& other) const final;

 private:
  friend class InterpolableLengthTest;

  // InterpolableValue:
  void Interpolate(const InterpolableValue& to,
                   const double progress,
                   InterpolableValue& result) const final;

  bool IsNumericLiteral() const { return type_ == Type::kNumericLiteral; }
  bool IsExpression() const { return type_ == Type::kExpression; }

  bool IsZeroLength() const {
    return IsNumericLiteral() && CSSPrimitiveValue::IsLength(unit_type_) &&
           single_value_ == 0;
  }

  void SetNumericLiteral(double value, CSSPrimitiveValue::UnitType unit_type);
  void SetNumericLiteral(const CSSNumericLiteralValue& value);
  void SetExpression(const CSSMathExpressionNode& expression);
  const CSSMathExpressionNode& AsExpression() const;

  enum class Type { kNumericLiteral, kExpression };
  Type type_;

  // Set when |type_| is |kNumericLiteral|.
  double single_value_;
  CSSPrimitiveValue::UnitType unit_type_;

  // Non-null when |type_| is |kExpression|.
  Persistent<const CSSMathExpressionNode> expression_;
};

template <>
struct DowncastTraits<InterpolableLength> {
  static bool AllowFrom(const InterpolableValue& interpolable_value) {
    return interpolable_value.IsLength();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_INTERPOLABLE_LENGTH_H_
