// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/stretchy_operator_shaper.h"

#include <hb-ot.h>
#include <hb.h>
#include <unicode/uchar.h>

#include "third_party/blink/renderer/platform/fonts/canvas_rotation_in_vertical.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/opentype/open_type_math_support.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_face.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_inline_headers.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"
#include "ui/gfx/skia_util.h"

namespace blink {

namespace {

// HarfBuzz' hb_position_t is a 16.16 fixed-point value.
inline float HarfBuzzUnitsToFloat(hb_position_t value) {
  static const float kFloatToHbRatio = 1.0f / (1 << 16);
  return kFloatToHbRatio * value;
}

inline float GetGlyphStretchSize(
    FloatRect bounds,
    OpenTypeMathStretchData::StretchAxis stretch_axis) {
  return stretch_axis == OpenTypeMathStretchData::StretchAxis::Horizontal
             ? bounds.Width()
             : bounds.Height();
}

inline StretchyOperatorShaper::Metrics ToMetrics(FloatRect bounds) {
  return {bounds.Width(), -bounds.Y(), bounds.MaxY()};
}

base::Optional<OpenTypeMathStretchData::AssemblyParameters>
GetAssemblyParameters(const HarfBuzzFace* harfbuzz_face,
                      Glyph base_glyph,
                      OpenTypeMathStretchData::StretchAxis stretch_axis,
                      float target_size) {
  Vector<OpenTypeMathStretchData::GlyphPartRecord> parts =
      OpenTypeMathSupport::GetGlyphPartRecords(harfbuzz_face, base_glyph,
                                               stretch_axis);
  if (parts.IsEmpty())
    return base::nullopt;

  hb_font_t* hb_font =
      harfbuzz_face->GetScaledFont(nullptr, HarfBuzzFace::NoVerticalLayout);

  auto hb_stretch_axis =
      stretch_axis == OpenTypeMathStretchData::StretchAxis::Horizontal
          ? HB_DIRECTION_LTR
          : HB_DIRECTION_BTT;

  // Go over the assembly parts and determine parameters used below.
  // https://mathml-refresh.github.io/mathml-core/#the-glyphassembly-table
  float min_connector_overlap = HarfBuzzUnitsToFloat(
      hb_ot_math_get_min_connector_overlap(hb_font, hb_stretch_axis));
  float max_connector_overlap = std::numeric_limits<float>::max();
  float non_extender_advance_sum = 0, extender_advance_sum = 0;
  unsigned non_extender_count = 0, extender_count = 0;

  for (auto& part : parts) {
    // Calculate the count and advance sums of extender and non-extender glyphs.
    if (part.is_extender) {
      extender_count++;
      extender_advance_sum += part.full_advance;
    } else {
      non_extender_count++;
      non_extender_advance_sum += part.full_advance;
    }

    // Take into account start connector length for all but the first glyph.
    if (part.is_extender || &part != &parts.front()) {
      max_connector_overlap =
          std::min(max_connector_overlap, part.start_connector_length);
    }

    // Take into account end connector length for all but the last glyph.
    if (part.is_extender || &part != &parts.back()) {
      max_connector_overlap =
          std::min(max_connector_overlap, part.end_connector_length);
    }
  }

  // Check validity conditions indicated in MathML core.
  float extender_non_overlapping_advance_sum =
      extender_advance_sum - min_connector_overlap * extender_count;
  if (extender_count == 0 || max_connector_overlap < min_connector_overlap ||
      extender_non_overlapping_advance_sum <= 0)
    return base::nullopt;

  // Calculate the minimal number of repetitions needed to obtain an assembly
  // size of size at least target size (r_min in MathML Core).
  unsigned repetition_count = std::max<float>(
      std::ceil((target_size - non_extender_advance_sum +
                 min_connector_overlap * (non_extender_count - 1)) /
                extender_non_overlapping_advance_sum),
      0);

  // Calculate the number of glyphs, limiting repetition_count to ensure the
  // assembly does not have more than HarfBuzzRunGlyphData::kMaxGlyphs.
  DCHECK_LE(non_extender_count, HarfBuzzRunGlyphData::kMaxGlyphs);
  repetition_count = std::min<unsigned>(
      repetition_count,
      (HarfBuzzRunGlyphData::kMaxGlyphs - non_extender_count) / extender_count);
  unsigned glyph_count = non_extender_count + repetition_count * extender_count;
  DCHECK_LE(glyph_count, HarfBuzzRunGlyphData::kMaxGlyphs);

  // Calculate the maximum overlap (called o_max in MathML Core) and the number
  // of glyph in such an assembly (called N in MathML Core).
  float connector_overlap = max_connector_overlap;
  if (glyph_count > 1) {
    float max_connector_overlap_theorical =
        (non_extender_advance_sum + repetition_count * extender_advance_sum -
         target_size) /
        (glyph_count - 1);
    connector_overlap =
        std::max(min_connector_overlap,
                 std::min(connector_overlap, max_connector_overlap_theorical));
  }

  // Calculate the assembly size (called  AssemblySize(o, r) in MathML Core).
  float stretch_size = non_extender_advance_sum +
                       repetition_count * extender_advance_sum -
                       connector_overlap * (glyph_count - 1);

  return base::Optional<OpenTypeMathStretchData::AssemblyParameters>(
      {connector_overlap, repetition_count, glyph_count, stretch_size,
       std::move(parts)});
}

}  // namespace

StretchyOperatorShaper::Metrics StretchyOperatorShaper::GetMetrics(
    const Font* font,
    float target_size) const {
  const SimpleFontData* primary_font = font->PrimaryFont();
  const HarfBuzzFace* harfbuzz_face =
      primary_font->PlatformData().GetHarfBuzzFace();
  Glyph base_glyph = primary_font->GlyphForCharacter(stretchy_character_);

  FloatRect bounds;

  // Try different glyph variants.
  for (auto& variant : OpenTypeMathSupport::GetGlyphVariantRecords(
           harfbuzz_face, base_glyph, stretch_axis_)) {
    bounds = primary_font->BoundsForGlyph(variant);
    if (GetGlyphStretchSize(bounds, stretch_axis_) >= target_size)
      return ToMetrics(bounds);
  }

  // Try a glyph assembly.
  auto params = GetAssemblyParameters(harfbuzz_face, base_glyph, stretch_axis_,
                                      target_size);
  if (!params)
    return ToMetrics(bounds);

  bounds = stretch_axis_ == OpenTypeMathStretchData::StretchAxis::Horizontal
               ? FloatRect(0, 0, params->stretch_size, 0)
               : FloatRect(0, -params->stretch_size, 0, params->stretch_size);

  for (auto& part : params->parts) {
    // Include dimension of the part, orthogonal to the stretch axis.
    auto glyph_bounds = primary_font->BoundsForGlyph(part.glyph);
    if (stretch_axis_ == OpenTypeMathStretchData::StretchAxis::Horizontal) {
      glyph_bounds.SetX(0);
      glyph_bounds.SetWidth(0);
    } else {
      glyph_bounds.SetY(0);
      glyph_bounds.SetHeight(0);
    }
    bounds.UniteEvenIfEmpty(glyph_bounds);
  }

  return ToMetrics(bounds);
}

scoped_refptr<ShapeResult> StretchyOperatorShaper::Shape(
    const Font* font,
    float target_size) const {
  const SimpleFontData* primary_font = font->PrimaryFont();
  const HarfBuzzFace* harfbuzz_face =
      primary_font->PlatformData().GetHarfBuzzFace();
  Glyph base_glyph = primary_font->GlyphForCharacter(stretchy_character_);

  Glyph glyph_variant;
  float glyph_variant_stretch_size;
  TextDirection direction = TextDirection::kLtr;

  // Try different glyph variants.
  for (auto& variant : OpenTypeMathSupport::GetGlyphVariantRecords(
           harfbuzz_face, base_glyph, stretch_axis_)) {
    glyph_variant = variant;
    auto bounds = primary_font->BoundsForGlyph(glyph_variant);
    glyph_variant_stretch_size = GetGlyphStretchSize(bounds, stretch_axis_);
    if (glyph_variant_stretch_size >= target_size) {
      return ShapeResult::CreateForStretchyMathOperator(
          font, direction, stretch_axis_, glyph_variant,
          glyph_variant_stretch_size);
    }
  }

  // Try a glyph assembly.
  auto params = GetAssemblyParameters(harfbuzz_face, base_glyph, stretch_axis_,
                                      target_size);
  if (!params) {
    return ShapeResult::CreateForStretchyMathOperator(
        font, direction, stretch_axis_, glyph_variant,
        glyph_variant_stretch_size);
  }

  return ShapeResult::CreateForStretchyMathOperator(
      font, direction, stretch_axis_, std::move(*params));
}

}  // namespace blink
