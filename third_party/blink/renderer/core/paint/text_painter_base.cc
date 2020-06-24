// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/text_painter_base.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/layout/text_decoration_offset_base.h"
#include "third_party/blink/renderer/core/paint/applied_decoration_painter.h"
#include "third_party/blink/renderer/core/paint/box_painter_base.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/selection_painting_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/shadow_list.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

namespace {

// We usually use the text decoration thickness to determine how far
// ink-skipped text decorations should be away from the glyph
// contours. Cap this at 5 CSS px in each direction when thickness
// growths larger than that. A value of 13 closely matches FireFox'
// implementation.
constexpr float kDecorationClipMaxDilation = 13;

static ResolvedUnderlinePosition ResolveUnderlinePosition(
    const ComputedStyle& style,
    FontBaseline baseline_type) {
  // |auto| should resolve to |under| to avoid drawing through glyphs in
  // scripts where it would not be appropriate (e.g., ideographs.)
  // However, this has performance implications. For now, we only work with
  // vertical text.
  switch (baseline_type) {
    case kAlphabeticBaseline:
      if (style.TextUnderlinePosition() & kTextUnderlinePositionUnder)
        return ResolvedUnderlinePosition::kUnder;
      if (style.TextUnderlinePosition() & kTextUnderlinePositionFromFont)
        return ResolvedUnderlinePosition::kNearAlphabeticBaselineFromFont;
      return ResolvedUnderlinePosition::kNearAlphabeticBaselineAuto;
    case kIdeographicBaseline:
      // Compute language-appropriate default underline position.
      // https://drafts.csswg.org/css-text-decor-3/#default-stylesheet
      UScriptCode script = style.GetFontDescription().GetScript();
      if (script == USCRIPT_KATAKANA_OR_HIRAGANA || script == USCRIPT_HANGUL) {
        if (style.TextUnderlinePosition() & kTextUnderlinePositionLeft) {
          return ResolvedUnderlinePosition::kUnder;
        }
        return ResolvedUnderlinePosition::kOver;
      }
      if (style.TextUnderlinePosition() & kTextUnderlinePositionRight) {
        return ResolvedUnderlinePosition::kOver;
      }
      return ResolvedUnderlinePosition::kUnder;
  }
  NOTREACHED();
  return ResolvedUnderlinePosition::kNearAlphabeticBaselineAuto;
}

static bool ShouldSetDecorationAntialias(const ComputedStyle& style) {
  for (const auto& decoration : style.AppliedTextDecorations()) {
    ETextDecorationStyle decoration_style = decoration.Style();
    if (decoration_style == ETextDecorationStyle::kDotted ||
        decoration_style == ETextDecorationStyle::kDashed)
      return true;
  }
  return false;
}

float ComputeDecorationThickness(
    const TextDecorationThickness text_decoration_thickness,
    const ComputedStyle& style,
    const SimpleFontData* font_data) {
  float auto_underline_thickness =
      std::max(1.f, style.ComputedFontSize() / 10.f);

  if (text_decoration_thickness.IsAuto())
    return auto_underline_thickness;

  // In principle we would not need to test for font_data if
  // |text_decoration_thickness.Thickness()| is fixed, but a null font_data here
  // would be a rare / error situation anyway, so practically, we can
  // early out here.
  if (!font_data)
    return auto_underline_thickness;

  if (text_decoration_thickness.IsFromFont()) {
    base::Optional<float> underline_thickness_font_metric =
        font_data->GetFontMetrics().UnderlineThickness().value();

    if (!underline_thickness_font_metric)
      return auto_underline_thickness;

    return std::max(1.f, underline_thickness_font_metric.value());
  }

  DCHECK(!text_decoration_thickness.IsFromFont());

  const Length& thickness_length = text_decoration_thickness.Thickness();
  float font_size = font_data->PlatformData().size();
  float text_decoration_thickness_pixels =
      FloatValueForLength(thickness_length, font_size);

  return std::max(1.f, text_decoration_thickness_pixels);
}

float DoubleOffsetFromThickness(float thickness_pixels) {
  return thickness_pixels + 1.0f;
}

}  // anonymous namespace

TextPainterBase::TextPainterBase(GraphicsContext& context,
                                 const Font& font,
                                 const PhysicalOffset& text_origin,
                                 const PhysicalRect& text_bounds,
                                 bool horizontal)
    : graphics_context_(context),
      font_(font),
      text_origin_(text_origin),
      text_bounds_(text_bounds),
      horizontal_(horizontal),
      has_combined_text_(false),
      emphasis_mark_offset_(0),
      ellipsis_offset_(0) {}

TextPainterBase::~TextPainterBase() = default;

void TextPainterBase::SetEmphasisMark(const AtomicString& emphasis_mark,
                                      TextEmphasisPosition position) {
  emphasis_mark_ = emphasis_mark;
  const SimpleFontData* font_data = font_.PrimaryFont();
  DCHECK(font_data);

  if (!font_data || emphasis_mark.IsNull()) {
    emphasis_mark_offset_ = 0;
  } else if ((horizontal_ && (position == TextEmphasisPosition::kOverRight ||
                              position == TextEmphasisPosition::kOverLeft)) ||
             (!horizontal_ &&
              (position == TextEmphasisPosition::kOverRight ||
               position == TextEmphasisPosition::kUnderRight))) {
    emphasis_mark_offset_ = -font_data->GetFontMetrics().Ascent() -
                            font_.EmphasisMarkDescent(emphasis_mark);
  } else {
    DCHECK(position == TextEmphasisPosition::kUnderRight ||
           position == TextEmphasisPosition::kUnderLeft ||
           position == TextEmphasisPosition::kOverLeft);
    emphasis_mark_offset_ = font_data->GetFontMetrics().Descent() +
                            font_.EmphasisMarkAscent(emphasis_mark);
  }
}

// static
void TextPainterBase::UpdateGraphicsContext(
    GraphicsContext& context,
    const TextPaintStyle& text_style,
    bool horizontal,
    GraphicsContextStateSaver& state_saver) {
  TextDrawingModeFlags mode = context.TextDrawingMode();
  if (text_style.stroke_width > 0) {
    TextDrawingModeFlags new_mode = mode | kTextModeStroke;
    if (mode != new_mode) {
      state_saver.SaveIfNeeded();
      context.SetTextDrawingMode(new_mode);
      mode = new_mode;
    }
  }

  if (mode & kTextModeFill && text_style.fill_color != context.FillColor())
    context.SetFillColor(text_style.fill_color);

  if (mode & kTextModeStroke) {
    if (text_style.stroke_color != context.StrokeColor())
      context.SetStrokeColor(text_style.stroke_color);
    if (text_style.stroke_width != context.StrokeThickness())
      context.SetStrokeThickness(text_style.stroke_width);
  }

  if (text_style.shadow) {
    state_saver.SaveIfNeeded();
    context.SetDrawLooper(text_style.shadow->CreateDrawLooper(
        DrawLooperBuilder::kShadowIgnoresAlpha, text_style.current_color,
        horizontal));
  }
}

Color TextPainterBase::TextColorForWhiteBackground(Color text_color) {
  int distance_from_white = DifferenceSquared(text_color, Color::kWhite);
  // semi-arbitrarily chose 65025 (255^2) value here after a few tests;
  return distance_from_white > 65025 ? text_color : text_color.Dark();
}

// static
TextPaintStyle TextPainterBase::TextPaintingStyle(const Document& document,
                                                  const ComputedStyle& style,
                                                  const PaintInfo& paint_info) {
  TextPaintStyle text_style;
  bool is_printing = paint_info.IsPrinting();

  if (paint_info.phase == PaintPhase::kTextClip) {
    // When we use the text as a clip, we only care about the alpha, thus we
    // make all the colors black.
    text_style.current_color = Color::kBlack;
    text_style.fill_color = Color::kBlack;
    text_style.stroke_color = Color::kBlack;
    text_style.emphasis_mark_color = Color::kBlack;
    text_style.stroke_width = style.TextStrokeWidth();
    text_style.shadow = nullptr;
  } else {
    text_style.current_color =
        style.VisitedDependentColor(GetCSSPropertyColor());
    text_style.fill_color =
        style.VisitedDependentColor(GetCSSPropertyWebkitTextFillColor());
    text_style.stroke_color =
        style.VisitedDependentColor(GetCSSPropertyWebkitTextStrokeColor());
    text_style.emphasis_mark_color =
        style.VisitedDependentColor(GetCSSPropertyWebkitTextEmphasisColor());
    text_style.stroke_width = style.TextStrokeWidth();
    text_style.shadow = style.TextShadow();

    // Adjust text color when printing with a white background.
    DCHECK_EQ(document.Printing(), is_printing);
    bool force_background_to_white =
        BoxPainterBase::ShouldForceWhiteBackgroundForPrintEconomy(document,
                                                                  style);
    if (force_background_to_white) {
      text_style.fill_color =
          TextColorForWhiteBackground(text_style.fill_color);
      text_style.stroke_color =
          TextColorForWhiteBackground(text_style.stroke_color);
      text_style.emphasis_mark_color =
          TextColorForWhiteBackground(text_style.emphasis_mark_color);
    }
  }

  return text_style;
}

TextPaintStyle TextPainterBase::SelectionPaintingStyle(
    const Document& document,
    const ComputedStyle& style,
    Node* node,
    bool have_selection,
    const PaintInfo& paint_info,
    const TextPaintStyle& text_style) {
  return SelectionPaintingUtils::SelectionPaintingStyle(
      document, style, node, have_selection, text_style, paint_info);
}

void TextPainterBase::DecorationsStripeIntercepts(
    float upper,
    float stripe_width,
    float dilation,
    const Vector<Font::TextIntercept>& text_intercepts) {
  for (auto intercept : text_intercepts) {
    FloatPoint clip_origin(text_origin_);
    FloatRect clip_rect(
        clip_origin + FloatPoint(intercept.begin_, upper),
        FloatSize(intercept.end_ - intercept.begin_, stripe_width));
    clip_rect.InflateX(dilation);
    // We need to ensure the clip rectangle is covering the full underline
    // extent. For horizontal drawing, using enclosingIntRect would be
    // sufficient, since we can clamp to full device pixels that way. However,
    // for vertical drawing, we have a transformation applied, which breaks the
    // integers-equal-device pixels assumption, so vertically inflating by 1
    // pixel makes sure we're always covering. This should only be done on the
    // clipping rectangle, not when computing the glyph intersects.
    clip_rect.InflateY(1.0);

    if (!clip_rect.IsFinite())
      continue;
    graphics_context_.ClipOut(clip_rect);
  }
}

void TextPainterBase::PaintDecorationsExceptLineThrough(
    const TextDecorationOffsetBase& decoration_offset,
    const DecorationInfo& decoration_info,
    const PaintInfo& paint_info,
    const Vector<AppliedTextDecoration>& decorations,
    const TextPaintStyle& text_style,
    bool* has_line_through_decoration) {
  GraphicsContext& context = paint_info.context;
  GraphicsContextStateSaver state_saver(context);
  UpdateGraphicsContext(context, text_style, horizontal_, state_saver);

  if (has_combined_text_)
    context.ConcatCTM(Rotation(text_bounds_, kClockwise));

  // text-underline-position may flip underline and overline.
  ResolvedUnderlinePosition underline_position =
      decoration_info.underline_position;
  bool flip_underline_and_overline = false;
  if (underline_position == ResolvedUnderlinePosition::kOver) {
    flip_underline_and_overline = true;
    underline_position = ResolvedUnderlinePosition::kUnder;
  }

  DCHECK_EQ(decorations.size(),
            decoration_info.applied_decorations_thickness.size());
  for (size_t applied_decorations_index = 0;
       applied_decorations_index < decorations.size();
       ++applied_decorations_index) {
    const AppliedTextDecoration& decoration =
        decorations[applied_decorations_index];
    TextDecoration lines = decoration.Lines();
    bool has_underline = EnumHasFlags(lines, TextDecoration::kUnderline);
    bool has_overline = EnumHasFlags(lines, TextDecoration::kOverline);
    if (flip_underline_and_overline)
      std::swap(has_underline, has_overline);

    float resolved_thickness =
        decoration_info
            .applied_decorations_thickness[applied_decorations_index];
    context.SetStrokeThickness(resolved_thickness);

    if (has_underline && decoration_info.font_data) {
      const int underline_offset = decoration_offset.ComputeUnderlineOffset(
          underline_position, decoration_info.font_data->GetFontMetrics(),
          resolved_thickness);
      PaintDecorationUnderOrOverLine(
          context, decoration_info, decoration, applied_decorations_index,
          underline_offset, DoubleOffsetFromThickness(resolved_thickness));
    }

    if (has_overline) {
      FontVerticalPositionType position =
          flip_underline_and_overline ? FontVerticalPositionType::TopOfEmHeight
                                      : FontVerticalPositionType::TextTop;
      const int overline_offset =
          decoration_offset.ComputeUnderlineOffsetForUnder(resolved_thickness,
                                                           position);
      PaintDecorationUnderOrOverLine(
          context, decoration_info, decoration, applied_decorations_index,
          overline_offset, -DoubleOffsetFromThickness(resolved_thickness));
    }

    // We could instead build a vector of the TextDecoration instances needing
    // line-through but this is a rare case so better to avoid vector overhead.
    *has_line_through_decoration |=
        EnumHasFlags(lines, TextDecoration::kLineThrough);
  }

  // Restore rotation as needed.
  if (has_combined_text_)
    context.ConcatCTM(Rotation(text_bounds_, kCounterclockwise));
}

void TextPainterBase::PaintDecorationsOnlyLineThrough(
    const DecorationInfo& decoration_info,
    const PaintInfo& paint_info,
    const Vector<AppliedTextDecoration>& decorations,
    const TextPaintStyle& text_style) {
  GraphicsContext& context = paint_info.context;
  GraphicsContextStateSaver state_saver(context);
  UpdateGraphicsContext(context, text_style, horizontal_, state_saver);

  if (has_combined_text_)
    context.ConcatCTM(Rotation(text_bounds_, kClockwise));

  DCHECK_EQ(decorations.size(),
            decoration_info.applied_decorations_thickness.size());
  for (size_t applied_decoration_index = 0;
       applied_decoration_index < decorations.size();
       ++applied_decoration_index) {
    const AppliedTextDecoration& decoration =
        decorations[applied_decoration_index];
    TextDecoration lines = decoration.Lines();
    if (EnumHasFlags(lines, TextDecoration::kLineThrough)) {
      float resolved_thickness =
          decoration_info
              .applied_decorations_thickness[applied_decoration_index];
      context.SetStrokeThickness(resolved_thickness);
      // For increased line thickness, the line-through decoration needs to grow
      // in both directions from its origin, subtract half the thickness to keep
      // it centered at the same origin.
      const float line_through_offset =
          2 * decoration_info.baseline / 3 - resolved_thickness / 2;
      // Floor double_offset in order to avoid double-line gap to appear
      // of different size depending on position where the double line
      // is drawn because of rounding downstream in
      // GraphicsContext::DrawLineForText.
      AppliedDecorationPainter decoration_painter(
          context, decoration_info, line_through_offset, decoration,
          applied_decoration_index,
          floorf(DoubleOffsetFromThickness(resolved_thickness)), 0);
      // No skip: ink for line-through,
      // compare https://github.com/w3c/csswg-drafts/issues/711
      decoration_painter.Paint();
    }
  }

  // Restore rotation as needed.
  if (has_combined_text_)
    context.ConcatCTM(Rotation(text_bounds_, kCounterclockwise));
}

void TextPainterBase::ComputeDecorationInfo(
    DecorationInfo& decoration_info,
    const PhysicalOffset& box_origin,
    PhysicalOffset local_origin,
    LayoutUnit width,
    FontBaseline baseline_type,
    const ComputedStyle& style,
    const ComputedStyle* decorating_box_style) {
  decoration_info.width = width;
  decoration_info.local_origin = FloatPoint(local_origin);
  decoration_info.antialias = ShouldSetDecorationAntialias(style);
  decoration_info.style = &style;
  decoration_info.baseline_type = baseline_type;
  decoration_info.underline_position = ResolveUnderlinePosition(
      *decoration_info.style, decoration_info.baseline_type);

  decoration_info.font_data = decoration_info.style->GetFont().PrimaryFont();
  DCHECK(decoration_info.font_data);
  decoration_info.baseline =
      decoration_info.font_data
          ? decoration_info.font_data->GetFontMetrics().FloatAscent()
          : 0;

  for (const AppliedTextDecoration& decoration :
       style.AppliedTextDecorations()) {
    decoration_info.applied_decorations_thickness.push_back(
        ComputeUnderlineThickness(decoration_info.underline_position,
                                  decoration.Thickness(), style,
                                  decorating_box_style));
  }
  DCHECK_EQ(style.AppliedTextDecorations().size(),
            decoration_info.applied_decorations_thickness.size());
}

float TextPainterBase::ComputeUnderlineThickness(
    const ResolvedUnderlinePosition& underline_position,
    const TextDecorationThickness& applied_decoration_thickness,
    const ComputedStyle& style,
    const ComputedStyle* decorating_box_style) {
  float thickness = 0;
  if ((underline_position ==
       ResolvedUnderlinePosition::kNearAlphabeticBaselineAuto) ||
      underline_position ==
          ResolvedUnderlinePosition::kNearAlphabeticBaselineFromFont) {
    thickness = ComputeDecorationThickness(applied_decoration_thickness, style,
                                           style.GetFont().PrimaryFont());
  } else {
    // Compute decorating box. Position and thickness are computed from the
    // decorating box.
    // Only for non-Roman for now for the performance implications.
    // https:// drafts.csswg.org/css-text-decor-3/#decorating-box
    if (decorating_box_style) {
      thickness = ComputeDecorationThickness(
          applied_decoration_thickness, *decorating_box_style,
          decorating_box_style->GetFont().PrimaryFont());
    } else {
      thickness = ComputeDecorationThickness(
          applied_decoration_thickness, style, style.GetFont().PrimaryFont());
    }
  }
  return thickness;
}

void TextPainterBase::PaintDecorationUnderOrOverLine(
    GraphicsContext& context,
    const DecorationInfo& decoration_info,
    const AppliedTextDecoration& decoration,
    size_t decoration_info_thickness_index,
    int line_offset,
    float decoration_offset) {
  AppliedDecorationPainter decoration_painter(
      context, decoration_info, line_offset, decoration,
      decoration_info_thickness_index, decoration_offset, 1);
  if (decoration_info.style->TextDecorationSkipInk() ==
      ETextDecorationSkipInk::kAuto) {
    FloatRect decoration_bounds = decoration_painter.Bounds();
    ClipDecorationsStripe(
        -decoration_info.baseline + decoration_bounds.Y() -
            decoration_info.local_origin.Y(),
        decoration_bounds.Height(),
        std::min(
            decoration_info
                .applied_decorations_thickness[decoration_info_thickness_index],
            kDecorationClipMaxDilation));
  }
  decoration_painter.Paint();
}

}  // namespace blink
