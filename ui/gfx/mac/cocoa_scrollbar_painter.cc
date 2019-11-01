// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mac/cocoa_scrollbar_painter.h"

#include "ui/gfx/canvas.h"
#include "ui/gfx/skia_util.h"

namespace gfx {

namespace {

// The width of the scroller track border.
constexpr int kTrackBorderWidth = 1;

// The amount the thumb is inset from the ends and the inside edge of track
// border.
constexpr int kThumbInset = 3;
constexpr int kThumbInsetOverlay = 2;

// Scrollbar thumb colors.
constexpr SkColor kThumbColorDefault = SkColorSetARGB(0x38, 0, 0, 0);
constexpr SkColor kThumbColorHover = SkColorSetARGB(0x80, 0, 0, 0);
constexpr SkColor kThumbColorDarkMode = SkColorSetRGB(0x6B, 0x6B, 0x6B);
constexpr SkColor kThumbColorDarkModeHover = SkColorSetRGB(0x93, 0x93, 0x93);
constexpr SkColor kThumbColorOverlay = SkColorSetARGB(0x80, 0, 0, 0);
constexpr SkColor kThumbColorOverlayDarkMode =
    SkColorSetARGB(0x80, 0xFF, 0xFF, 0xFF);

// Non-overlay scroller track colors are not transparent. On Safari, they are,
// but on all other macOS applications they are not.
constexpr SkColor kTrackGradientColors[] = {
    SkColorSetRGB(0xF9, 0xF9, 0xF9),
    SkColorSetRGB(0xF9, 0xF9, 0xF9),
};
constexpr SkColor kTrackInnerBorderColor = SkColorSetRGB(0xE8, 0xE8, 0xE8);
constexpr SkColor kTrackOuterBorderColor = SkColorSetRGB(0xED, 0xED, 0xED);

// Non-overlay dark mode scroller track colors.
constexpr SkColor kTrackGradientColorsDarkMode[] = {
    SkColorSetRGB(0x2D, 0x2D, 0x2D),
    SkColorSetRGB(0x2B, 0x2B, 0x2B),
};
constexpr SkColor kTrackInnerBorderColorDarkMode =
    SkColorSetRGB(0x3D, 0x3D, 0x3D);
constexpr SkColor kTrackOuterBorderColorDarkMode =
    SkColorSetRGB(0x51, 0x51, 0x51);

// Overlay scroller track colors are transparent.
constexpr SkColor kTrackGradientColorsOverlay[] = {
    SkColorSetARGB(0xC6, 0xF8, 0xF8, 0xF8),
    SkColorSetARGB(0xC2, 0xF8, 0xF8, 0xF8),
    SkColorSetARGB(0xC2, 0xF8, 0xF8, 0xF8),
    SkColorSetARGB(0xC2, 0xF8, 0xF8, 0xF8),
};
constexpr SkColor kTrackInnerBorderColorOverlay =
    SkColorSetARGB(0xF9, 0xDF, 0xDF, 0xDF);
constexpr SkColor kTrackOuterBorderColorOverlay =
    SkColorSetARGB(0xC6, 0xE8, 0xE8, 0xE8);

// Dark mode overlay scroller track colors.
constexpr SkColor kTrackGradientColorsOverlayDarkMode[] = {
    SkColorSetARGB(0x28, 0xD8, 0xD8, 0xD8),
    SkColorSetARGB(0x26, 0xCC, 0xCC, 0xCC),
    SkColorSetARGB(0x26, 0xCC, 0xCC, 0xCC),
    SkColorSetARGB(0x26, 0xCC, 0xCC, 0xCC),
};
constexpr SkColor kTrackInnerBorderColorOverlayDarkMode =
    SkColorSetARGB(0x33, 0xE5, 0xE5, 0xE5);
constexpr SkColor kTrackOuterBorderColorOverlayDarkMode =
    SkColorSetARGB(0x28, 0xD8, 0xD8, 0xD8);

}  // namespace

// static
void CocoaScrollbarPainter::PaintTrack(gfx::Canvas* canvas,
                                       const gfx::Rect& track_rect,
                                       const Params& params) {
  // Set the colors based on overlay and dark mode.
  SkColor inner_border_color = kTrackInnerBorderColor;
  SkColor outer_border_color = kTrackOuterBorderColor;
  const SkColor* gradient_colors = kTrackGradientColors;
  size_t gradient_stops = base::size(kTrackGradientColors);
  if (params.overlay) {
    if (params.dark_mode) {
      inner_border_color = kTrackInnerBorderColorOverlayDarkMode;
      outer_border_color = kTrackOuterBorderColorOverlayDarkMode;
      gradient_colors = kTrackGradientColorsOverlayDarkMode;
      gradient_stops = base::size(kTrackGradientColorsOverlayDarkMode);
    } else {
      inner_border_color = kTrackInnerBorderColorOverlay;
      outer_border_color = kTrackOuterBorderColorOverlay;
      gradient_colors = kTrackGradientColorsOverlay;
      gradient_stops = base::size(kTrackGradientColorsOverlay);
    }
  } else {
    if (params.dark_mode) {
      inner_border_color = kTrackInnerBorderColorDarkMode;
      outer_border_color = kTrackOuterBorderColorDarkMode;
      gradient_colors = kTrackGradientColorsDarkMode;
      gradient_stops = base::size(kTrackGradientColorsDarkMode);
    }
  }

  // Paint the scrollbar track background.
  {
    const SkPoint gradient_bounds[] = {
        gfx::PointToSkPoint(track_rect.origin()),
        gfx::PointToSkPoint(params.orientation == Orientation::kHorizontal
                                ? track_rect.bottom_left()
                                : track_rect.top_right()),
    };
    cc::PaintFlags gradient;
    gradient.setShader(cc::PaintShader::MakeLinearGradient(
        gradient_bounds, gradient_colors, nullptr, gradient_stops,
        SkTileMode::kClamp));
    canvas->DrawRect(track_rect, gradient);
  }

  // Draw the inner border: top if horizontal, left if vertical.
  {
    cc::PaintFlags flags;
    flags.setColor(inner_border_color);
    gfx::Rect inner_border(track_rect);
    if (params.orientation == Orientation::kHorizontal)
      inner_border.set_height(kTrackBorderWidth);
    else
      inner_border.set_width(kTrackBorderWidth);
    canvas->DrawRect(inner_border, flags);
  }

  // Draw the outer border: bottom if horizontal, right if veritcal.
  {
    cc::PaintFlags flags;
    flags.setColor(outer_border_color);
    gfx::Rect outer_border(track_rect);
    if (params.orientation == Orientation::kHorizontal) {
      outer_border.set_height(kTrackBorderWidth);
      outer_border.set_y(track_rect.bottom() - kTrackBorderWidth);
    } else {
      outer_border.set_width(kTrackBorderWidth);
      outer_border.set_x(track_rect.right() - kTrackBorderWidth);
    }
    canvas->DrawRect(outer_border, flags);
  }
}

// static
void CocoaScrollbarPainter::PaintThumb(gfx::Canvas* canvas,
                                       const gfx::Rect& thumb_bounds,
                                       const Params& params) {
  gfx::Rect bounds(thumb_bounds);
  SkColor thumb_color = 0;
  if (params.overlay) {
    bounds.Inset(kThumbInsetOverlay, kThumbInsetOverlay);
    if (params.dark_mode)
      thumb_color = kThumbColorOverlayDarkMode;
    else
      thumb_color = kThumbColorOverlay;
  } else {
    bounds.Inset(kThumbInset, kThumbInset);
    if (params.dark_mode) {
      if (params.hovered)
        thumb_color = kThumbColorDarkModeHover;
      else
        thumb_color = kThumbColorDarkMode;
    } else {
      if (params.hovered)
        thumb_color = kThumbColorHover;
      else
        thumb_color = kThumbColorDefault;
    }
  }

  switch (params.orientation) {
    case Orientation::kHorizontal:
      bounds.Inset(0, kTrackBorderWidth, 0, 0);
      break;
    case Orientation::kVerticalOnLeft:
      bounds.Inset(0, 0, kTrackBorderWidth, 0);
      break;
    case Orientation::kVerticalOnRight:
      bounds.Inset(kTrackBorderWidth, 0, 0, 0);
      break;
  }

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(thumb_color);
  const SkScalar radius = std::min(bounds.width(), bounds.height());
  canvas->DrawRoundRect(bounds, radius, flags);
}

}  // namespace gfx
