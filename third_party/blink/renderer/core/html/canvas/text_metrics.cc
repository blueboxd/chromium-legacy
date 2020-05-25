// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/text_metrics.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_baselines.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_bidi_paragraph.h"
#include "third_party/blink/renderer/platform/fonts/character_range.h"

namespace blink {

constexpr int kHangingAsPercentOfAscent = 80;

float TextMetrics::GetFontBaseline(const TextBaseline& text_baseline,
                                   const SimpleFontData& font_data) {
  FontMetrics font_metrics = font_data.GetFontMetrics();
  switch (text_baseline) {
    case kTopTextBaseline:
      return font_data.EmHeightAscent().ToFloat();
    case kHangingTextBaseline:
      // According to
      // http://wiki.apache.org/xmlgraphics-fop/LineLayout/AlignmentHandling
      // "FOP (Formatting Objects Processor) puts the hanging baseline at 80% of
      // the ascender height"
      return font_metrics.FloatAscent() * kHangingAsPercentOfAscent / 100.0;
    case kIdeographicTextBaseline:
      return -font_metrics.FloatDescent();
    case kBottomTextBaseline:
      return -font_data.EmHeightDescent().ToFloat();
    case kMiddleTextBaseline:
      return (font_data.EmHeightAscent().ToFloat() -
              font_data.EmHeightDescent().ToFloat()) /
             2.0f;
    case kAlphabeticTextBaseline:
    default:
      // Do nothing.
      break;
  }
  return 0;
}

void TextMetrics::Trace(Visitor* visitor) const {
  visitor->Trace(baselines_);
  ScriptWrappable::Trace(visitor);
}

TextMetrics::TextMetrics() : baselines_(Baselines::Create()) {}

TextMetrics::TextMetrics(const Font& font,
                         const TextDirection& direction,
                         const TextBaseline& baseline,
                         const TextAlign& align,
                         const String& text)
    : TextMetrics() {
  Update(font, direction, baseline, align, text);
}

void TextMetrics::Update(const Font& font,
                         const TextDirection& direction,
                         const TextBaseline& baseline,
                         const TextAlign& align,
                         const String& text) {
  const SimpleFontData* font_data = font.PrimaryFont();
  if (!font_data)
    return;

  // TODO(kojii): Need to figure out the desired behavior of |advances| when
  // bidi reorder occurs.
  TextRun text_run(
      text, /* xpos */ 0, /* expansion */ 0,
      TextRun::kAllowTrailingExpansion | TextRun::kForbidLeadingExpansion,
      direction, false);
  text_run.SetNormalizeSpace(true);
  advances_ = font.IndividualCharacterAdvances(text_run);

  // x direction
  // Run bidi algorithm on the given text. Step 5 of:
  // https://html.spec.whatwg.org/multipage/canvas.html#text-preparation-algorithm
  FloatRect glyph_bounds;
  String text16 = text;
  text16.Ensure16Bit();
  NGBidiParagraph bidi;
  bidi.SetParagraph(text16, direction);
  NGBidiParagraph::Runs runs;
  bidi.GetLogicalRuns(text16, &runs);
  float xpos = 0;
  for (const auto& run : runs) {
    // Measure each run.
    TextRun text_run(
        StringView(text, run.start, run.Length()), xpos, /* expansion */ 0,
        TextRun::kAllowTrailingExpansion | TextRun::kForbidLeadingExpansion,
        run.Direction(), /* directional_override */ false);
    text_run.SetNormalizeSpace(true);
    FloatRect run_glyph_bounds;
    float run_width = font.Width(text_run, nullptr, &run_glyph_bounds);

    // Accumulate the position and the glyph bounding box.
    run_glyph_bounds.Move(xpos, 0);
    glyph_bounds.Unite(run_glyph_bounds);
    xpos += run_width;
  }
  double real_width = xpos;
#if DCHECK_IS_ON()
  // This DCHECK is for limited time only; to use |glyph_bounds| instead of
  // |BoundingBox| and make sure they are compatible.
  if (runs.size() == 1 && direction == runs[0].Direction()) {
    FloatRect bbox = font.BoundingBox(text_run);
    // |GetCharacterRange|, the underlying function of |BoundingBox|, clamps
    // negative |MaxY| to 0. This is unintentional, and that we are not copying
    // the behavior.
    DCHECK_EQ(bbox.Y(), std::min(glyph_bounds.Y(), .0f));
    DCHECK_EQ(bbox.MaxY(), std::max(glyph_bounds.MaxY(), .0f));
    DCHECK_EQ(bbox.Width(), real_width);
  }
#endif
  width_ = real_width;

  float dx = 0.0f;
  if (align == kCenterTextAlign)
    dx = real_width / 2.0f;
  else if (align == kRightTextAlign ||
           (align == kStartTextAlign && direction == TextDirection::kRtl) ||
           (align == kEndTextAlign && direction != TextDirection::kRtl))
    dx = real_width;
  actual_bounding_box_left_ = -glyph_bounds.X() + dx;
  actual_bounding_box_right_ = glyph_bounds.MaxX() - dx;

  // y direction
  const FontMetrics& font_metrics = font_data->GetFontMetrics();
  const float ascent = font_metrics.FloatAscent();
  const float descent = font_metrics.FloatDescent();
  const float baseline_y = GetFontBaseline(baseline, *font_data);
  font_bounding_box_ascent_ = ascent - baseline_y;
  font_bounding_box_descent_ = descent + baseline_y;
  actual_bounding_box_ascent_ = -glyph_bounds.Y() - baseline_y;
  actual_bounding_box_descent_ = glyph_bounds.MaxY() + baseline_y;
  em_height_ascent_ = font_data->EmHeightAscent() - baseline_y;
  em_height_descent_ = font_data->EmHeightDescent() + baseline_y;

  // TODO(fserb): hanging/ideographic baselines are broken.
  baselines_->setAlphabetic(-baseline_y);
  baselines_->setHanging(ascent * kHangingAsPercentOfAscent / 100.0f -
                         baseline_y);
  baselines_->setIdeographic(-descent - baseline_y);
}

}  // namespace blink
