// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_aura.h"

#include <limits>
#include <utility>

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/layout.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/skia_util.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme_features.h"
#include "ui/native_theme/overlay_scrollbar_constants_aura.h"

namespace ui {

namespace {

// Constants for painting overlay scrollbars. Other properties needed outside
// this painting code are defined in overlay_scrollbar_constants_aura.h.
constexpr int kOverlayScrollbarMinimumLength = 32;

// 2 pixel border with 1 pixel center patch. The border is 2 pixels despite the
// stroke width being 1 so that the inner pixel can match the center tile
// color. This prevents color interpolation between the patches.
constexpr int kOverlayScrollbarBorderPatchWidth = 2;
constexpr int kOverlayScrollbarCenterPatchSize = 1;

const SkColor kTrackColor = SkColorSetRGB(0xF1, 0xF1, 0xF1);

const int kCheckboxBorderRadius = 2;
const int kCheckboxAndRadioBorderWidth = 1;
const SkColor kCheckboxAndRadioTinyColor = SK_ColorGRAY;
const SkColor kCkeckboxAndRadioBackgroundColor =
    SkColorSetRGB(0xFF, 0xFF, 0xFF);
const SkColor kCheckboxAndRadioBorderColor = SkColorSetRGB(0xCE, 0xCE, 0xCE);
const SkColor kCheckboxAndRadioBorderHoveredColor =
    SkColorSetRGB(0x9D, 0x9D, 0x9D);
const SkColor kCheckboxAndRadioBorderDisabledColor =
    SkColorSetRGB(0xC5, 0xC5, 0xC5);
const SkColor kCheckboxAndRadioStrokeColor = SkColorSetRGB(0x73, 0x73, 0x73);
const SkColor kCheckboxAndRadioStrokeDisabledColor =
    SkColorSetRGB(0xC5, 0xC5, 0xC5);

const int kInputBorderRadius = 2;
const int kInputBorderWidth = 1;
const SkColor kInputBorderColor = SkColorSetRGB(0xCE, 0xCE, 0xCE);
const SkColor kInputBorderHoveredColor = SkColorSetRGB(0x9D, 0x9D, 0x9D);
const SkColor kInputBorderDisabledColor = SkColorSetRGB(0xC5, 0xC5, 0xC5);

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// NativeTheme:

#if !defined(OS_MACOSX)
// static
NativeTheme* NativeTheme::GetInstanceForWeb() {
  return NativeThemeAura::web_instance();
}

#if !defined(OS_WIN)
// static
NativeTheme* NativeTheme::GetInstanceForNativeUi() {
  return NativeThemeAura::instance();
}
#endif  // OS_WIN
#endif  // !OS_MACOSX

////////////////////////////////////////////////////////////////////////////////
// NativeThemeAura:

NativeThemeAura::NativeThemeAura(bool use_overlay_scrollbars)
    : use_overlay_scrollbars_(use_overlay_scrollbars) {
// We don't draw scrollbar buttons.
#if defined(OS_CHROMEOS)
  set_scrollbar_button_length(0);
#endif

  if (use_overlay_scrollbars_) {
    scrollbar_width_ =
        kOverlayScrollbarThumbWidthPressed + kOverlayScrollbarStrokeWidth;
  }

  // Images and alphas declarations assume the following order.
  static_assert(kDisabled == 0, "states unexpectedly changed");
  static_assert(kHovered == 1, "states unexpectedly changed");
  static_assert(kNormal == 2, "states unexpectedly changed");
  static_assert(kPressed == 3, "states unexpectedly changed");
}

NativeThemeAura::~NativeThemeAura() {}

// static
NativeThemeAura* NativeThemeAura::instance() {
  static base::NoDestructor<NativeThemeAura> s_native_theme(false);
  return s_native_theme.get();
}

// static
NativeThemeAura* NativeThemeAura::web_instance() {
  static base::NoDestructor<NativeThemeAura> s_native_theme_for_web(
      IsOverlayScrollbarEnabled());
  return s_native_theme_for_web.get();
}

// This implementation returns hardcoded colors.
SkColor NativeThemeAura::GetSystemColor(ColorId color_id) const {
  return GetAuraColor(color_id, this);
}

void NativeThemeAura::PaintMenuPopupBackground(
    cc::PaintCanvas* canvas,
    const gfx::Size& size,
    const MenuBackgroundExtraParams& menu_background) const {
  SkColor color = GetSystemColor(NativeTheme::kColorId_MenuBackgroundColor);
  if (menu_background.corner_radius > 0) {
    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setAntiAlias(true);
    flags.setColor(color);

    SkPath path;
    SkRect rect = SkRect::MakeWH(SkIntToScalar(size.width()),
                                 SkIntToScalar(size.height()));
    SkScalar radius = SkIntToScalar(menu_background.corner_radius);
    SkScalar radii[8] = {radius, radius, radius, radius,
                         radius, radius, radius, radius};
    path.addRoundRect(rect, radii);

    canvas->drawPath(path, flags);
  } else {
    canvas->drawColor(color, SkBlendMode::kSrc);
  }
}

void NativeThemeAura::PaintMenuItemBackground(
    cc::PaintCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const MenuItemExtraParams& menu_item) const {
  CommonThemePaintMenuItemBackground(this, canvas, state, rect, menu_item);
}

void NativeThemeAura::PaintArrowButton(cc::PaintCanvas* canvas,
                                       const gfx::Rect& rect,
                                       Part direction,
                                       State state) const {
  SkColor bg_color = kTrackColor;
  // Aura-win uses slightly different arrow colors.
  SkColor arrow_color = gfx::kPlaceholderColor;
  switch (state) {
    case kDisabled:
      arrow_color = GetArrowColor(state);
      break;
    case kHovered:
      bg_color = SkColorSetRGB(0xD2, 0xD2, 0xD2);
      FALLTHROUGH;
    case kNormal:
      arrow_color = SkColorSetRGB(0x50, 0x50, 0x50);
      break;
    case kPressed:
      bg_color = SkColorSetRGB(0x78, 0x78, 0x78);
      arrow_color = SK_ColorWHITE;
      break;
    case kNumStates:
      break;
  }
  DCHECK_NE(arrow_color, gfx::kPlaceholderColor);

  cc::PaintFlags flags;
  flags.setColor(bg_color);
  canvas->drawIRect(gfx::RectToSkIRect(rect), flags);

  PaintArrow(canvas, rect, direction, arrow_color);
}

void NativeThemeAura::PaintScrollbarTrack(
    cc::PaintCanvas* canvas,
    Part part,
    State state,
    const ScrollbarTrackExtraParams& extra_params,
    const gfx::Rect& rect) const {
  // Overlay Scrollbar should never paint a scrollbar track.
  DCHECK(!use_overlay_scrollbars_);
  cc::PaintFlags flags;
  flags.setColor(kTrackColor);
  canvas->drawIRect(gfx::RectToSkIRect(rect), flags);
}

void NativeThemeAura::PaintScrollbarThumb(
    cc::PaintCanvas* canvas,
    Part part,
    State state,
    const gfx::Rect& rect,
    ScrollbarOverlayColorTheme theme) const {
  // Do not paint if state is disabled.
  if (state == kDisabled)
    return;

  TRACE_EVENT0("blink", "NativeThemeAura::PaintScrollbarThumb");

  SkAlpha thumb_alpha = SK_AlphaTRANSPARENT;
  gfx::Rect thumb_rect(rect);
  SkColor thumb_color;

  if (use_overlay_scrollbars_) {
    // Indexed by ScrollbarOverlayColorTheme.
    constexpr SkColor kOverlayScrollbarThumbColor[] = {SK_ColorBLACK,
                                                       SK_ColorWHITE};
    constexpr SkColor kOverlayScrollbarStrokeColor[] = {SK_ColorWHITE,
                                                        SK_ColorBLACK};

    thumb_color = kOverlayScrollbarThumbColor[theme];

    SkAlpha stroke_alpha = SK_AlphaTRANSPARENT;
    switch (state) {
      case NativeTheme::kDisabled:
        thumb_alpha = SK_AlphaTRANSPARENT;
        stroke_alpha = SK_AlphaTRANSPARENT;
        break;
      case NativeTheme::kHovered:
        thumb_alpha = SK_AlphaOPAQUE * kOverlayScrollbarThumbHoverAlpha;
        stroke_alpha = SK_AlphaOPAQUE * kOverlayScrollbarStrokeHoverAlpha;
        break;
      case NativeTheme::kNormal:
        thumb_alpha = SK_AlphaOPAQUE * kOverlayScrollbarThumbNormalAlpha;
        stroke_alpha = SK_AlphaOPAQUE * kOverlayScrollbarStrokeNormalAlpha;
        break;
      case NativeTheme::kPressed:
        thumb_alpha = SK_AlphaOPAQUE * kOverlayScrollbarThumbHoverAlpha;
        stroke_alpha = SK_AlphaOPAQUE * kOverlayScrollbarStrokeHoverAlpha;
        break;
      case NativeTheme::kNumStates:
        NOTREACHED();
        break;
    }

    // In overlay mode, draw a stroke (border).
    constexpr int kStrokeWidth = kOverlayScrollbarStrokeWidth;
    cc::PaintFlags flags;
    flags.setColor(
        SkColorSetA(kOverlayScrollbarStrokeColor[theme], stroke_alpha));
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(kStrokeWidth);

    gfx::RectF stroke_rect(thumb_rect);
    gfx::InsetsF stroke_insets(kStrokeWidth / 2.f);
    // The edge to which the scrollbar is attached shouldn't have a border.
    gfx::Insets edge_adjust_insets;
    if (part == NativeTheme::kScrollbarHorizontalThumb)
      edge_adjust_insets = gfx::Insets(0, 0, -kStrokeWidth, 0);
    else
      edge_adjust_insets = gfx::Insets(0, 0, 0, -kStrokeWidth);
    stroke_rect.Inset(stroke_insets + edge_adjust_insets);
    canvas->drawRect(gfx::RectFToSkRect(stroke_rect), flags);

    // Inset the all the edges edges so we fill-in the stroke below.
    // For left vertical scrollbar, we will horizontally flip the canvas in
    // ScrollbarThemeOverlay::paintThumb.
    gfx::Insets fill_insets(kStrokeWidth);
    thumb_rect.Inset(fill_insets + edge_adjust_insets);
  } else {
    switch (state) {
      case NativeTheme::kDisabled:
        thumb_alpha = SK_AlphaTRANSPARENT;
        break;
      case NativeTheme::kHovered:
        thumb_alpha = 0x4D;
        break;
      case NativeTheme::kNormal:
        thumb_alpha = 0x33;
        break;
      case NativeTheme::kPressed:
        thumb_alpha = 0x80;
        break;
      case NativeTheme::kNumStates:
        NOTREACHED();
        break;
    }
    // If there are no scrollbuttons then provide some padding so that the thumb
    // doesn't touch the top of the track.
    const int kThumbPadding = 2;
    const int extra_padding =
        (scrollbar_button_length() == 0) ? kThumbPadding : 0;
    if (part == NativeTheme::kScrollbarVerticalThumb)
      thumb_rect.Inset(kThumbPadding, extra_padding);
    else
      thumb_rect.Inset(extra_padding, kThumbPadding);

    thumb_color = SK_ColorBLACK;
  }

  cc::PaintFlags flags;
  flags.setColor(SkColorSetA(thumb_color, thumb_alpha));
  canvas->drawIRect(gfx::RectToSkIRect(thumb_rect), flags);
}

void NativeThemeAura::PaintScrollbarCorner(cc::PaintCanvas* canvas,
                                           State state,
                                           const gfx::Rect& rect) const {
  // Overlay Scrollbar should never paint a scrollbar corner.
  DCHECK(!use_overlay_scrollbars_);
  cc::PaintFlags flags;
  flags.setColor(SkColorSetRGB(0xDC, 0xDC, 0xDC));
  canvas->drawIRect(RectToSkIRect(rect), flags);
}

void NativeThemeAura::PaintCheckbox(cc::PaintCanvas* canvas,
                                    State state,
                                    const gfx::Rect& rect,
                                    const ButtonExtraParams& button) const {
  if (!features::IsFormControlsRefreshEnabled()) {
    return NativeThemeBase::PaintCheckbox(canvas, state, rect, button);
  }

  SkRect skrect = PaintCheckboxRadioCommon(
      canvas, state, rect, SkIntToScalar(kCheckboxBorderRadius));

  if (!skrect.isEmpty()) {
    // Draw the checkmark / dash.
    cc::PaintFlags flags;
    flags.setAntiAlias(true);

    if (state == kDisabled) {
      flags.setColor(kCheckboxAndRadioStrokeDisabledColor);
    } else {
      flags.setColor(kCheckboxAndRadioStrokeColor);
    }

    if (button.indeterminate) {
      const auto indeterminate =
          skrect.makeInset(skrect.width() * 0.2, skrect.height() * 0.2);
      flags.setStyle(cc::PaintFlags::kFill_Style);
      canvas->drawRoundRect(indeterminate, SkIntToScalar(kCheckboxBorderRadius),
                            SkIntToScalar(kCheckboxBorderRadius), flags);
    } else if (button.checked) {
      SkPath check;
      check.moveTo(skrect.x() + skrect.width() * 0.2, skrect.centerY());
      check.rLineTo(skrect.width() * 0.2, skrect.height() * 0.2);
      check.lineTo(skrect.right() - skrect.width() * 0.2,
                   skrect.y() + skrect.height() * 0.2);
      flags.setStyle(cc::PaintFlags::kStroke_Style);
      flags.setStrokeWidth(SkFloatToScalar(skrect.height() * 0.16));
      canvas->drawPath(check, flags);
    }
  }
}

void NativeThemeAura::PaintRadio(cc::PaintCanvas* canvas,
                                 State state,
                                 const gfx::Rect& rect,
                                 const ButtonExtraParams& button) const {
  if (!features::IsFormControlsRefreshEnabled()) {
    return NativeThemeBase::PaintRadio(canvas, state, rect, button);
  }

  // Most of a radio button is the same as a checkbox, except the the rounded
  // square is a circle (i.e. border radius >= 100%).
  const SkScalar radius = SkFloatToScalar(
      static_cast<float>(std::max(rect.width(), rect.height())) * 0.5);

  SkRect skrect = PaintCheckboxRadioCommon(canvas, state, rect, radius);
  if (!skrect.isEmpty() && button.checked) {
    // Draw the dot.
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    if (state == kDisabled) {
      flags.setColor(kCheckboxAndRadioStrokeDisabledColor);
    } else {
      flags.setColor(kCheckboxAndRadioStrokeColor);
    }
    skrect.inset(skrect.width() * 0.2, skrect.height() * 0.2);
    // Use drawRoundedRect instead of drawOval to be completely consistent
    // with the border in PaintCheckboxRadioNewCommon.
    canvas->drawRoundRect(skrect, radius, radius, flags);
  }
}

// Draws the common elements of checkboxes and radio buttons.
// Returns the rectangle within which any additional decorations should be
// drawn, or empty if none.
SkRect NativeThemeAura::PaintCheckboxRadioCommon(
    cc::PaintCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const SkScalar borderRadius) const {
  SkRect skrect = gfx::RectToSkRect(rect);

  // Use the largest square that fits inside the provided rectangle.
  // No other browser seems to support non-square widget, so accidentally
  // having non-square sizes is common (eg. amazon and webkit dev tools).
  if (skrect.width() != skrect.height()) {
    SkScalar size = SkMinScalar(skrect.width(), skrect.height());
    skrect.inset((skrect.width() - size) / 2, (skrect.height() - size) / 2);
  }

  // If the rectangle is too small then paint only a rectangle. We don't want
  // to have to worry about '- 1' and '+ 1' calculations below having overflow
  // or underflow.
  if (skrect.width() <= 2) {
    cc::PaintFlags flags;
    flags.setColor(kCheckboxAndRadioTinyColor);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->drawRect(skrect, flags);
    // Too small to draw anything more.
    return SkRect::MakeEmpty();
  }

  cc::PaintFlags flags;
  flags.setAntiAlias(true);

  const SkScalar borderWidth = SkIntToScalar(kCheckboxAndRadioBorderWidth);

  // Paint the background (is not visible behind the rounded corners).
  skrect.inset(borderWidth / 2, borderWidth / 2);
  flags.setColor(kCkeckboxAndRadioBackgroundColor);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->drawRoundRect(skrect, borderRadius, borderRadius, flags);

  // Draw the border.
  if (state == kHovered) {
    flags.setColor(kCheckboxAndRadioBorderHoveredColor);
  } else if (state == kDisabled) {
    flags.setColor(kCheckboxAndRadioBorderDisabledColor);
  } else {
    flags.setColor(kCheckboxAndRadioBorderColor);
  }
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(borderWidth);
  canvas->drawRoundRect(skrect, borderRadius, borderRadius, flags);

  // Return the rectangle for drawing any additional decorations.
  return skrect;
}

void NativeThemeAura::PaintTextField(cc::PaintCanvas* canvas,
                                     State state,
                                     const gfx::Rect& rect,
                                     const TextFieldExtraParams& text) const {
  if (!features::IsFormControlsRefreshEnabled()) {
    return NativeThemeBase::PaintTextField(canvas, state, rect, text);
  }

  SkRect bounds = gfx::RectToSkRect(rect);
  const SkScalar borderWidth = SkIntToScalar(kInputBorderWidth);

  // Paint the background (is not visible behind the rounded corners).
  bounds.inset(borderWidth / 2, borderWidth / 2);
  cc::PaintFlags fill_flags;
  fill_flags.setStyle(cc::PaintFlags::kFill_Style);
  fill_flags.setColor(text.background_color);
  canvas->drawRoundRect(bounds, SkIntToScalar(kInputBorderRadius),
                        SkIntToScalar(kInputBorderRadius), fill_flags);

  // Paint the border: 1px solid.
  cc::PaintFlags stroke_flags;
  if (state == kHovered) {
    stroke_flags.setColor(kInputBorderHoveredColor);
  } else if (state == kDisabled) {
    stroke_flags.setColor(kInputBorderDisabledColor);
  } else {
    stroke_flags.setColor(kInputBorderColor);
  }
  stroke_flags.setStyle(cc::PaintFlags::kStroke_Style);
  stroke_flags.setStrokeWidth(borderWidth);
  canvas->drawRoundRect(bounds, SkIntToScalar(kInputBorderRadius),
                        SkIntToScalar(kInputBorderRadius), stroke_flags);
}

gfx::Size NativeThemeAura::GetPartSize(Part part,
                                       State state,
                                       const ExtraParams& extra) const {
  if (use_overlay_scrollbars_) {
    constexpr int minimum_length =
        kOverlayScrollbarMinimumLength + 2 * kOverlayScrollbarStrokeWidth;

    // Aura overlay scrollbars need a slight tweak from the base sizes.
    switch (part) {
      case kScrollbarHorizontalThumb:
        return gfx::Size(minimum_length, scrollbar_width_);
      case kScrollbarVerticalThumb:
        return gfx::Size(scrollbar_width_, minimum_length);

      default:
        // TODO(bokan): We should probably make sure code using overlay
        // scrollbars isn't asking for part sizes that don't exist.
        // crbug.com/657159.
        break;
    }
  }

  return NativeThemeBase::GetPartSize(part, state, extra);
}

bool NativeThemeAura::SupportsNinePatch(Part part) const {
  if (!IsOverlayScrollbarEnabled())
    return false;

  return part == kScrollbarHorizontalThumb || part == kScrollbarVerticalThumb;
}

gfx::Size NativeThemeAura::GetNinePatchCanvasSize(Part part) const {
  DCHECK(SupportsNinePatch(part));

  return gfx::Size(
      kOverlayScrollbarBorderPatchWidth * 2 + kOverlayScrollbarCenterPatchSize,
      kOverlayScrollbarBorderPatchWidth * 2 + kOverlayScrollbarCenterPatchSize);
}

gfx::Rect NativeThemeAura::GetNinePatchAperture(Part part) const {
  DCHECK(SupportsNinePatch(part));

  return gfx::Rect(
      kOverlayScrollbarBorderPatchWidth, kOverlayScrollbarBorderPatchWidth,
      kOverlayScrollbarCenterPatchSize, kOverlayScrollbarCenterPatchSize);
}

}  // namespace ui
