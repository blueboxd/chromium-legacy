// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"

#include "third_party/blink/renderer/core/css/css_value_pool.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

struct SameSizeAsCSSNumericLiteralValue : CSSPrimitiveValue {
  double num;
};
ASSERT_SIZE(CSSNumericLiteralValue, SameSizeAsCSSNumericLiteralValue);

void CSSNumericLiteralValue::TraceAfterDispatch(blink::Visitor* visitor) {
  CSSPrimitiveValue::TraceAfterDispatch(visitor);
}

CSSNumericLiteralValue::CSSNumericLiteralValue(double num, UnitType type)
    : CSSPrimitiveValue(type, kNumericLiteralClass), num_(num) {
  DCHECK(std::isfinite(num));
}

// static
CSSNumericLiteralValue* CSSNumericLiteralValue::Create(double value,
                                                       UnitType type) {
  // TODO(timloh): This looks wrong.
  if (std::isinf(value))
    return nullptr;

  if (value < 0 || value > CSSValuePool::kMaximumCacheableIntegerValue)
    return MakeGarbageCollected<CSSNumericLiteralValue>(value, type);

  int int_value = clampTo<int>(value);
  if (value != int_value)
    return MakeGarbageCollected<CSSNumericLiteralValue>(value, type);

  // TODO(xiaochengh): Make |CSSValuePool| cache |CSSNumericLiteralValue| to
  // avoid type casting.
  CSSValuePool& pool = CssValuePool();
  CSSPrimitiveValue* result = nullptr;
  switch (type) {
    case CSSPrimitiveValue::UnitType::kPixels:
      result = pool.PixelCacheValue(int_value);
      if (!result) {
        result = pool.SetPixelCacheValue(
            int_value,
            MakeGarbageCollected<CSSNumericLiteralValue>(value, type));
      }
      return To<CSSNumericLiteralValue>(result);
    case CSSPrimitiveValue::UnitType::kPercentage:
      result = pool.PercentCacheValue(int_value);
      if (!result) {
        result = pool.SetPercentCacheValue(
            int_value,
            MakeGarbageCollected<CSSNumericLiteralValue>(value, type));
      }
      return To<CSSNumericLiteralValue>(result);
    case CSSPrimitiveValue::UnitType::kNumber:
    case CSSPrimitiveValue::UnitType::kInteger:
      result = pool.NumberCacheValue(int_value);
      if (!result) {
        result = pool.SetNumberCacheValue(
            int_value, MakeGarbageCollected<CSSNumericLiteralValue>(
                           value, CSSPrimitiveValue::UnitType::kInteger));
      }
      return To<CSSNumericLiteralValue>(result);
    default:
      return MakeGarbageCollected<CSSNumericLiteralValue>(value, type);
  }
}

}  // namespace blink
