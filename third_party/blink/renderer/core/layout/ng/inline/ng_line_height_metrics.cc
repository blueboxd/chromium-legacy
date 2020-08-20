// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_height_metrics.h"

#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

FontHeight::FontHeight(const ComputedStyle& style, FontBaseline baseline_type) {
  const SimpleFontData* font_data = style.GetFont().PrimaryFont();
  DCHECK(font_data);
  // TODO(kojii): This should not be null, but it happens. Avoid crash for now.
  if (font_data)
    Initialize(font_data->GetFontMetrics(), baseline_type);
}

FontHeight::FontHeight(const ComputedStyle& style)
    : FontHeight(style, style.GetFontBaseline()) {}

FontHeight::FontHeight(const FontMetrics& font_metrics,
                       FontBaseline baseline_type) {
  Initialize(font_metrics, baseline_type);
}

void FontHeight::Initialize(const FontMetrics& font_metrics,
                            FontBaseline baseline_type) {
  // TODO(kojii): In future, we'd like to use LayoutUnit metrics to support
  // sub-CSS-pixel layout.
  ascent = LayoutUnit(font_metrics.Ascent(baseline_type));
  descent = LayoutUnit(font_metrics.Descent(baseline_type));
}

void FontHeight::AddLeading(LayoutUnit line_height) {
  DCHECK(!IsEmpty());
  LayoutUnit half_leading = (line_height - (ascent + descent)) / 2;
  // TODO(kojii): floor() is to make text dump compatible with legacy test
  // results. Revisit when we paint.
  ascent += half_leading.Floor();
  descent = line_height - ascent;
}

void FontHeight::Move(LayoutUnit delta) {
  DCHECK(!IsEmpty());
  ascent -= delta;
  descent += delta;
}

void FontHeight::Unite(const FontHeight& other) {
  ascent = std::max(ascent, other.ascent);
  descent = std::max(descent, other.descent);
}

void FontHeight::operator+=(const FontHeight& other) {
  DCHECK(ascent != LayoutUnit::Min() && descent != LayoutUnit::Min());
  DCHECK(other.ascent != LayoutUnit::Min() &&
         other.descent != LayoutUnit::Min());
  ascent += other.ascent;
  descent += other.descent;
}

std::ostream& operator<<(std::ostream& stream, const FontHeight& metrics) {
  return stream << "ascent=" << metrics.ascent
                << ", descent=" << metrics.descent;
}

}  // namespace blink
