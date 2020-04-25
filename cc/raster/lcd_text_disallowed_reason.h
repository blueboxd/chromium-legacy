// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_LCD_TEXT_DISALLOWED_REASON_H_
#define CC_RASTER_LCD_TEXT_DISALLOWED_REASON_H_

#include <cstddef>
#include <cstdint>
#include <iosfwd>

#include "cc/cc_export.h"

namespace cc {

enum class LCDTextDisallowedReason : uint8_t {
  kNone,
  kSetting,
  kBackgroundColorNotOpaque,
  kContentsNotOpaque,
  kNonIntegralTranslation,
  kNonIntegralXOffset,
  kNonIntegralYOffset,
  kWillChangeTransform,
};
constexpr size_t kLCDTextDisallowedReasonCount =
    static_cast<size_t>(LCDTextDisallowedReason::kWillChangeTransform) + 1;
CC_EXPORT const char* LCDTextDisallowedReasonToString(LCDTextDisallowedReason);

CC_EXPORT std::ostream& operator<<(std::ostream&, LCDTextDisallowedReason);

}  // namespace cc

#endif  // CC_RASTER_LCD_TEXT_DISALLOWED_REASON_H_
