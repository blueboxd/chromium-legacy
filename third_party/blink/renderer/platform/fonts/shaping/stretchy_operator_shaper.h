// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_STRETCHY_OPERATOR_SHAPER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_STRETCHY_OPERATOR_SHAPER_H_

#include <base/memory/scoped_refptr.h>
#include <unicode/uchar.h>
#include "third_party/blink/renderer/platform/fonts/glyph.h"
#include "third_party/blink/renderer/platform/fonts/opentype/open_type_math_support.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Font;
class ShapeResult;
class StretchyOperatorShaper;

// TODO(https://crbug.com/1057589): Add a TextDirection parameter, so that it's
// possible to perform glyph-level (rtlm feature) or character-level mirroring
// before stretching.
// https://mathml-refresh.github.io/mathml-core/#algorithms-for-glyph-stretching
class PLATFORM_EXPORT StretchyOperatorShaper final {
  DISALLOW_NEW();

 public:
  StretchyOperatorShaper(UChar stretchy_character,
                         OpenTypeMathStretchData::StretchAxis stretch_axis)
      : stretchy_character_(stretchy_character), stretch_axis_(stretch_axis) {}

  // Returns the metrics of the stretched operator for layout purpose.
  // May be called multiple times; font and direction may vary between calls.
  // https://mathml-refresh.github.io/mathml-core/#dfn-box-metrics-of-a-stretchy-glyph
  struct Metrics {
    float advance;
    float ascent;
    float descent;
    // TODO(https://crbug.com/1057592): Add italic correction.
  };
  Metrics GetMetrics(const Font*, float target_size) const;

  // Shape the stretched operator. The coordinates of the glyph(s) use the same
  // origin as the rectangle returned by GetMetrics.
  // May be called multiple times; font and direction may vary between calls.
  // https://mathml-refresh.github.io/mathml-core/#dfn-shape-a-stretchy-glyph
  scoped_refptr<ShapeResult> Shape(const Font*, float target_size) const;

  ~StretchyOperatorShaper() = default;

 private:
  const UChar stretchy_character_;
  const OpenTypeMathStretchData::StretchAxis stretch_axis_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_STRETCHY_OPERATOR_SHAPER_H_
