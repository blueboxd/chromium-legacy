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
#include "ui/gfx/rrect_f.h"
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
const SkScalar kScrollRadius =
    1;  // select[multiple] radius+width are set in css

const int kCheckboxBorderRadius = 2;
const int kCheckboxAndRadioBorderWidth = 1;

const int kInputBorderRadius = 2;
const int kInputBorderWidth = 1;
const SkScalar kButtonBorderRadius = 2.f;
const SkScalar kButtonBorderWidth = 1.f;

const SkScalar kSliderTrackRadius = 40.f;

const int kSliderThumbWidth = 21;
const int kSliderThumbHeight = 21;
const SkScalar kSliderThumbBorderWidth = 2.f;
const SkScalar kSliderThumbBorderHoveredWidth = 4.f;

const SkScalar kMenuListArrowStrokeWidth = 1.f;

// The "dash" is 8x2 px by default (the checkbox is 13x13 px).
const SkScalar kIndeterminateInsetWidthRatio = (13 - 8) / 2.0f / 13;
const SkScalar kIndeterminateInsetHeightRatio = (13 - 2) / 2.0f / 13;
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

SkColor NativeThemeAura::GetSystemColor(ColorId color_id,
                                        ColorScheme color_scheme) const {
  return GetAuraColor(color_id, this, color_scheme);
}

SkColor NativeThemeAura::GetControlColor(ControlColorId color_id,
                                         ColorScheme color_scheme) const {
  if (UsesHighContrastColors())
    return GetHighContrastControlColor(color_id, color_scheme);

  switch (color_id) {
    case kBorder:
      return SkColorSetRGB(0x76, 0x76, 0x76);
    case kHoveredBorder:
      return SkColorSetRGB(0x4F, 0x4F, 0x4F);
    case kDisabledBorder:
      return SkColorSetARGB(0x4D, 0x76, 0x76, 0x76);
    case kAccent:
      return SkColorSetRGB(0x00, 0x75, 0xFF);
    case kHoveredAccent:
      return SkColorSetRGB(0x00, 0x5C, 0xC8);
    case kDisabledAccent:
      return SkColorSetARGB(0x4D, 0x76, 0x76, 0x76);
    case kBackground:
      return SK_ColorWHITE;
    case kDisabledBackground:
      return SkColorSetA(SK_ColorWHITE, 0x99);
    case kFill:
      return SkColorSetRGB(0xEF, 0xEF, 0xEF);
    case kHoveredFill:
      return SkColorSetRGB(0xE5, 0xE5, 0xE5);
    case kDisabledFill:
      return SkColorSetARGB(0x4D, 0xEF, 0xEF, 0xEF);
    case kLightenLayer:
      return SkColorSetARGB(0x33, 0xA9, 0xA9, 0xA9);
  }
  NOTREACHED();
  return gfx::kPlaceholderColor;
}

SkColor NativeThemeAura::GetHighContrastControlColor(
    ControlColorId color_id,
    ColorScheme color_scheme) const {
  if (!system_colors_.empty()) {
    switch (color_id) {
      case kDisabledBorder:
      case kDisabledAccent:
        return system_colors_[SystemThemeColor::kGrayText];
      case kBorder:
      case kHoveredBorder:
        return system_colors_[SystemThemeColor::kButtonText];
      case kAccent:
      case kHoveredAccent:
        return system_colors_[SystemThemeColor::kHighlight];
      case kBackground:
      case kDisabledBackground:
      case kFill:
      case kHoveredFill:
      case kDisabledFill:
      case kLightenLayer:
        return system_colors_[SystemThemeColor::kWindow];
    }
  } else {
    // Default high contrast colors (used in web test mode)
    switch (color_id) {
      case kDisabledBorder:
      case kDisabledAccent:
        return SK_ColorGREEN;
      case kBorder:
      case kHoveredBorder:
        return SK_ColorWHITE;
      case kAccent:
      case kHoveredAccent:
        return SK_ColorCYAN;
      case kBackground:
      case kDisabledBackground:
      case kFill:
      case kHoveredFill:
      case kDisabledFill:
      case kLightenLayer:
        return SK_ColorBLACK;
    }
  }
  NOTREACHED();
  return gfx::kPlaceholderColor;
}

void NativeThemeAura::PaintMenuPopupBackground(
    cc::PaintCanvas* canvas,
    const gfx::Size& size,
    const MenuBackgroundExtraParams& menu_background,
    ColorScheme color_scheme) const {
  SkColor color =
      GetSystemColor(NativeTheme::kColorId_MenuBackgroundColor, color_scheme);
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
    const MenuItemExtraParams& menu_item,
    ColorScheme color_scheme) const {
  CommonThemePaintMenuItemBackground(this, canvas, state, rect, menu_item,
                                     color_scheme);
}

void NativeThemeAura::PaintArrowButton(
    cc::PaintCanvas* canvas,
    const gfx::Rect& rect,
    Part direction,
    State state,
    ColorScheme color_scheme,
    const ScrollbarArrowExtraParams& arrow) const {
  SkColor bg_color = kTrackColor;
  // Aura-win uses slightly different arrow colors.
  SkColor arrow_color = gfx::kPlaceholderColor;
  switch (state) {
    case kDisabled:
      arrow_color = GetArrowColor(state, color_scheme);
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

  if (!features::IsFormControlsRefreshEnabled()) {
    canvas->drawIRect(gfx::RectToSkIRect(rect), flags);

    return PaintArrow(canvas, rect, direction, arrow_color);
  }

  SkScalar upper_left_radius = 0;
  SkScalar lower_left_radius = 0;
  SkScalar upper_right_radius = 0;
  SkScalar lower_right_radius = 0;
  float zoom = arrow.zoom ? arrow.zoom : 1.0;
  if (direction == kScrollbarUpArrow) {
    static_assert(kInputBorderRadius > 0, "no border radius present");
    if (arrow.right_to_left) {
      upper_left_radius = kScrollRadius * zoom;
    } else {
      upper_right_radius = kScrollRadius * zoom;
    }
  } else if (direction == kScrollbarDownArrow) {
    static_assert(kInputBorderRadius > 0, "no border radius present");
    if (arrow.right_to_left) {
      lower_left_radius = kScrollRadius * zoom;
    } else {
      lower_right_radius = kScrollRadius * zoom;
    }
  }
  DrawPartiallyRoundRect(canvas, rect, upper_left_radius, upper_right_radius,
                         lower_right_radius, lower_left_radius, flags);

  PaintArrow(canvas, rect, direction, arrow_color);
}

void NativeThemeAura::PaintScrollbarTrack(
    cc::PaintCanvas* canvas,
    Part part,
    State state,
    const ScrollbarTrackExtraParams& extra_params,
    const gfx::Rect& rect,
    ColorScheme color_scheme) const {
  // Overlay Scrollbar should never paint a scrollbar track.
  DCHECK(!use_overlay_scrollbars_);
  cc::PaintFlags flags;
  flags.setColor(kTrackColor);
  canvas->drawIRect(gfx::RectToSkIRect(rect), flags);
}

void NativeThemeAura::PaintScrollbarThumb(cc::PaintCanvas* canvas,
                                          Part part,
                                          State state,
                                          const gfx::Rect& rect,
                                          ScrollbarOverlayColorTheme theme,
                                          ColorScheme color_scheme) const {
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
                                           const gfx::Rect& rect,
                                           ColorScheme color_scheme) const {
  // Overlay Scrollbar should never paint a scrollbar corner.
  DCHECK(!use_overlay_scrollbars_);
  cc::PaintFlags flags;
  flags.setColor(SkColorSetRGB(0xDC, 0xDC, 0xDC));
  canvas->drawIRect(RectToSkIRect(rect), flags);
}

SkColor NativeThemeAura::ControlsAccentColorForState(
    State state,
    ColorScheme color_scheme) const {
  ControlColorId color_id;
  if (state == kHovered) {
    color_id = kHoveredAccent;
  } else if (state == kPressed) {
    color_id = kHoveredAccent;
  } else if (state == kDisabled) {
    color_id = kDisabledAccent;
  } else {
    color_id = kAccent;
  }
  return GetControlColor(color_id, color_scheme);
}

SkColor NativeThemeAura::ControlsBorderColorForState(
    State state,
    ColorScheme color_scheme) const {
  ControlColorId color_id;
  if (state == kHovered) {
    color_id = kHoveredBorder;
  } else if (state == kDisabled) {
    color_id = kDisabledBorder;
  } else {
    color_id = kBorder;
  }
  return GetControlColor(color_id, color_scheme);
}

SkColor NativeThemeAura::ControlsFillColorForState(
    State state,
    ColorScheme color_scheme) const {
  ControlColorId color_id;
  if (state == kHovered) {
    color_id = kHoveredFill;
  } else if (state == kPressed) {
    color_id = kHoveredFill;
  } else if (state == kDisabled) {
    color_id = kDisabledFill;
  } else {
    color_id = kFill;
  }
  return GetControlColor(color_id, color_scheme);
}

SkColor NativeThemeAura::ControlsBackgroundColorForState(
    State state,
    ColorScheme color_scheme) const {
  ControlColorId color_id;
  if (state == kDisabled) {
    color_id = kDisabledBackground;
  } else {
    color_id = kBackground;
  }
  return GetControlColor(color_id, color_scheme);
}

void NativeThemeAura::PaintCheckbox(cc::PaintCanvas* canvas,
                                    State state,
                                    const gfx::Rect& rect,
                                    const ButtonExtraParams& button,
                                    ColorScheme color_scheme) const {
  if (!features::IsFormControlsRefreshEnabled()) {
    return NativeThemeBase::PaintCheckbox(canvas, state, rect, button,
                                          color_scheme);
  }

  SkRect skrect = PaintCheckboxRadioCommon(canvas, state, rect, button, true,
                                           SkIntToScalar(kCheckboxBorderRadius),
                                           color_scheme);

  if (!skrect.isEmpty()) {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);

    if (button.indeterminate) {
      // Draw the dash.
      flags.setColor(ControlsBorderColorForState(state, color_scheme));
      const auto indeterminate =
          skrect.makeInset(skrect.width() * kIndeterminateInsetWidthRatio,
                           skrect.height() * kIndeterminateInsetHeightRatio);
      flags.setStyle(cc::PaintFlags::kFill_Style);
      canvas->drawRoundRect(indeterminate, SkIntToScalar(kCheckboxBorderRadius),
                            SkIntToScalar(kCheckboxBorderRadius), flags);
    } else if (button.checked) {
      // Draw the accent background.
      flags.setStyle(cc::PaintFlags::kFill_Style);
      flags.setColor(ControlsAccentColorForState(state, color_scheme));
      canvas->drawRoundRect(skrect, kCheckboxBorderRadius,
                            kCheckboxBorderRadius, flags);

      // Draw the checkmark.
      SkPath check;
      check.moveTo(skrect.x() + skrect.width() * 0.2, skrect.centerY());
      check.rLineTo(skrect.width() * 0.2, skrect.height() * 0.2);
      check.lineTo(skrect.right() - skrect.width() * 0.2,
                   skrect.y() + skrect.height() * 0.2);
      flags.setStyle(cc::PaintFlags::kStroke_Style);
      flags.setStrokeWidth(SkFloatToScalar(skrect.height() * 0.16));
      SkColor checkmark_color =
          ControlsBackgroundColorForState(state, color_scheme);
      flags.setColor(checkmark_color);
      canvas->drawPath(check, flags);
    }
  }
}

void NativeThemeAura::PaintRadio(cc::PaintCanvas* canvas,
                                 State state,
                                 const gfx::Rect& rect,
                                 const ButtonExtraParams& button,
                                 ColorScheme color_scheme) const {
  if (!features::IsFormControlsRefreshEnabled()) {
    return NativeThemeBase::PaintRadio(canvas, state, rect, button,
                                       color_scheme);
  }

  // Most of a radio button is the same as a checkbox, except the the rounded
  // square is a circle (i.e. border radius >= 100%).
  const SkScalar radius = SkFloatToScalar(
      static_cast<float>(std::max(rect.width(), rect.height())) * 0.5);

  SkRect skrect = PaintCheckboxRadioCommon(canvas, state, rect, button, false,
                                           radius, color_scheme);
  if (!skrect.isEmpty() && button.checked) {
    // Draw the dot.
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(ControlsAccentColorForState(state, color_scheme));

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
    const ButtonExtraParams& button,
    bool is_checkbox,
    const SkScalar borderRadius,
    ColorScheme color_scheme) const {
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
    flags.setColor(GetControlColor(kBorder, color_scheme));
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->drawRect(skrect, flags);
    // Too small to draw anything more.
    return SkRect::MakeEmpty();
  }

  cc::PaintFlags flags;
  flags.setAntiAlias(true);

  const SkScalar borderWidth = SkIntToScalar(kCheckboxAndRadioBorderWidth);

  // Paint the background (is not visible behind the rounded corners).
  // Note we need to shrink the rect for background a little bit so we don't see
  // artifacts introduced by antialiasing between the border and the background
  // near the rounded corners of checkbox.
  const auto background_rect =
      skrect.makeInset(borderWidth * 0.2f, borderWidth * 0.2f);
  PaintLightenLayer(canvas, background_rect, state, borderRadius, color_scheme);
  flags.setColor(ControlsBackgroundColorForState(state, color_scheme));
  flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->drawRoundRect(background_rect, borderRadius, borderRadius, flags);

  // Draw the border.
  if (!(is_checkbox && button.checked)) {
    // Shrink half border width so the final pixels of the border will be within
    // the rectangle.
    const auto border_rect = skrect.makeInset(borderWidth / 2, borderWidth / 2);
    SkColor border_color =
        button.checked ? ControlsAccentColorForState(state, color_scheme)
                       : ControlsBorderColorForState(state, color_scheme);
    flags.setColor(border_color);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(borderWidth);
    canvas->drawRoundRect(border_rect, borderRadius, borderRadius, flags);
  }

  // Return the rectangle for drawing any additional decorations.
  return skrect;
}

void NativeThemeAura::PaintTextField(cc::PaintCanvas* canvas,
                                     State state,
                                     const gfx::Rect& rect,
                                     const TextFieldExtraParams& text,
                                     ColorScheme color_scheme) const {
  if (!features::IsFormControlsRefreshEnabled()) {
    return NativeThemeBase::PaintTextField(canvas, state, rect, text,
                                           color_scheme);
  }

  SkRect bounds = gfx::RectToSkRect(rect);
  const SkScalar borderWidth = SkIntToScalar(kInputBorderWidth);
  const SkScalar borderRadius = SkIntToScalar(kInputBorderRadius);

  // Paint the background (is not visible behind the rounded corners).
  bounds.inset(borderWidth / 2, borderWidth / 2);
  cc::PaintFlags fill_flags;
  fill_flags.setStyle(cc::PaintFlags::kFill_Style);
  if (text.background_color != 0) {
    PaintLightenLayer(canvas, bounds, state, borderRadius, color_scheme);
    fill_flags.setColor(ControlsBackgroundColorForState(state, color_scheme));
    canvas->drawRoundRect(bounds, borderRadius, borderRadius, fill_flags);
  }

  // Paint the border: 1px solid.
  cc::PaintFlags stroke_flags;
  stroke_flags.setColor(ControlsBorderColorForState(state, color_scheme));
  stroke_flags.setStyle(cc::PaintFlags::kStroke_Style);
  stroke_flags.setStrokeWidth(borderWidth);
  canvas->drawRoundRect(bounds, borderRadius, borderRadius, stroke_flags);
}

void NativeThemeAura::PaintLightenLayer(cc::PaintCanvas* canvas,
                                        SkRect skrect,
                                        State state,
                                        SkScalar border_radius,
                                        ColorScheme color_scheme) const {
  if (state == kDisabled) {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    // Draw the lighten layer to lighten the background so the translucent
    // disabled color works regardless of what it's over.
    flags.setColor(GetControlColor(kLightenLayer, color_scheme));
    canvas->drawRoundRect(skrect, border_radius, border_radius, flags);
  }
}

void NativeThemeAura::PaintButton(cc::PaintCanvas* canvas,
                                  State state,
                                  const gfx::Rect& rect,
                                  const ButtonExtraParams& button,
                                  ColorScheme color_scheme) const {
  if (!features::IsFormControlsRefreshEnabled()) {
    return NativeThemeBase::PaintButton(canvas, state, rect, button,
                                        color_scheme);
  }

  cc::PaintFlags flags;
  SkRect skrect = gfx::RectToSkRect(rect);

  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);

  // If the button is too small, fallback to drawing a single, solid color.
  if (rect.width() < 5 || rect.height() < 5) {
    flags.setColor(ControlsFillColorForState(state, color_scheme));
    canvas->drawRect(skrect, flags);
    return;
  }

  // Paint the background (is not visible behind the rounded corners).
  skrect.inset(kButtonBorderWidth / 2, kButtonBorderWidth / 2);
  PaintLightenLayer(canvas, skrect, state, kButtonBorderRadius, color_scheme);
  flags.setColor(ControlsFillColorForState(state, color_scheme));
  canvas->drawRoundRect(skrect, kButtonBorderRadius, kButtonBorderRadius,
                        flags);

  // Paint the border: 1px solid.
  if (button.has_border) {
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(kButtonBorderWidth);
    flags.setColor(ControlsBorderColorForState(state, color_scheme));
    canvas->drawRoundRect(skrect, kButtonBorderRadius, kButtonBorderRadius,
                          flags);
  }
}

SkRect AlignSliderTrack(const gfx::Rect& slider_rect,
                        const NativeTheme::SliderExtraParams& slider,
                        bool is_value) {
  const int kAlignment = 2;
  const int mid_x = slider_rect.x() + slider_rect.width() / 2;
  const int mid_y = slider_rect.y() + slider_rect.height() / 2;
  SkRect aligned_rect;

  if (slider.vertical) {
    const int top = is_value ? slider_rect.y() + slider.thumb_y + kAlignment
                             : slider_rect.y();
    aligned_rect.setLTRB(std::max(slider_rect.x(), mid_x - kAlignment), top,
                         std::min(slider_rect.right(), mid_x + kAlignment),
                         slider_rect.bottom());
  } else {
    const int right = is_value ? slider_rect.x() + slider.thumb_x + kAlignment
                               : slider_rect.right();
    aligned_rect.setLTRB(slider_rect.x(),
                         std::max(slider_rect.y(), mid_y - kAlignment), right,
                         std::min(slider_rect.bottom(), mid_y + kAlignment));
  }

  return aligned_rect;
}

void NativeThemeAura::PaintSliderTrack(cc::PaintCanvas* canvas,
                                       State state,
                                       const gfx::Rect& rect,
                                       const SliderExtraParams& slider,
                                       ColorScheme color_scheme) const {
  if (!features::IsFormControlsRefreshEnabled()) {
    return NativeThemeBase::PaintSliderTrack(canvas, state, rect, slider,
                                             color_scheme);
  }

  // Paint the entire slider track.
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(GetControlColor(kFill, color_scheme));
  SkRect track_rect = AlignSliderTrack(rect, slider, false);
  canvas->drawRoundRect(track_rect, kSliderTrackRadius, kSliderTrackRadius,
                        flags);

  // Paint the value slider track.
  flags.setColor(ControlsAccentColorForState(state, color_scheme));
  SkRect value_rect = AlignSliderTrack(rect, slider, true);
  canvas->drawRoundRect(value_rect, kSliderTrackRadius, kSliderTrackRadius,
                        flags);
}

void NativeThemeAura::PaintSliderThumb(cc::PaintCanvas* canvas,
                                       State state,
                                       const gfx::Rect& rect,
                                       const SliderExtraParams& slider,
                                       ColorScheme color_scheme) const {
  if (!features::IsFormControlsRefreshEnabled()) {
    return NativeThemeBase::PaintSliderThumb(canvas, state, rect, slider,
                                             color_scheme);
  }

  const SkScalar radius = SkFloatToScalar(
      static_cast<float>(std::max(rect.width(), rect.height())) * 0.5);
  SkRect thumb_rect = gfx::RectToSkRect(rect);

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  SkScalar border_width = kSliderThumbBorderWidth;
  if (state == kHovered || state == kPressed) {
    border_width = kSliderThumbBorderHoveredWidth;
  }

  // Paint the background (is not visible behind the rounded corners).
  thumb_rect.inset(border_width / 2, border_width / 2);
  flags.setColor(GetControlColor(kFill, color_scheme));
  flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->drawRoundRect(thumb_rect, radius, radius, flags);

  // Paint the border.
  flags.setColor(ControlsBorderColorForState(state, color_scheme));
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(border_width);
  canvas->drawRoundRect(thumb_rect, radius, radius, flags);
}

void NativeThemeAura::PaintMenuList(cc::PaintCanvas* canvas,
                                    State state,
                                    const gfx::Rect& rect,
                                    const MenuListExtraParams& menu_list,
                                    ColorScheme color_scheme) const {
  if (!features::IsFormControlsRefreshEnabled()) {
    return NativeThemeBase::PaintMenuList(canvas, state, rect, menu_list,
                                          color_scheme);
  }

  // If a border radius is specified paint the background and the border of
  // the menulist, otherwise let the non-theming code paint the background
  // and the border of the control. The arrow (menulist button) is always
  // painted by the theming code.
  if (!menu_list.has_border_radius) {
    TextFieldExtraParams text_field = {0};
    text_field.background_color = menu_list.background_color;
    PaintTextField(canvas, state, rect, text_field, color_scheme);
  }

  // Paint the arrow.
  cc::PaintFlags flags;
  flags.setColor(menu_list.arrow_color);
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(kMenuListArrowStrokeWidth);

  float arrow_width = menu_list.arrow_size;
  int arrow_height = arrow_width * 0.6;
  gfx::Rect arrow(menu_list.arrow_x, menu_list.arrow_y - (arrow_height / 2),
                  arrow_width, arrow_height);
  arrow.Intersect(rect);

  if (arrow_width != arrow.width() || arrow_height != arrow.height()) {
    // The arrow is clipped after being constrained to the paint rect so we need
    // to recalculate its size.
    int height_clip = arrow_height - arrow.height();
    int width_clip = arrow_width - arrow.width();
    if (height_clip > width_clip) {
      arrow.set_width(arrow.height() * 1.6);
    } else {
      arrow.set_height(arrow.width() * 0.6);
    }
    arrow.set_y(menu_list.arrow_y - (arrow.height() / 2));
  }

  SkPath path;
  path.moveTo(arrow.x(), arrow.y());
  path.lineTo(arrow.x() + arrow.width() / 2, arrow.y() + arrow.height());
  path.lineTo(arrow.x() + arrow.width(), arrow.y());
  canvas->drawPath(path, flags);
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

  if (part == kSliderThumb && features::IsFormControlsRefreshEnabled()) {
    return gfx::Size(kSliderThumbWidth, kSliderThumbHeight);
  }

  return NativeThemeBase::GetPartSize(part, state, extra);
}

void NativeThemeAura::DrawPartiallyRoundRect(cc::PaintCanvas* canvas,
                                             const gfx::Rect& rect,
                                             const SkScalar upper_left_radius,
                                             const SkScalar upper_right_radius,
                                             const SkScalar lower_right_radius,
                                             const SkScalar lower_left_radius,
                                             const cc::PaintFlags& flags) {
  gfx::RRectF rounded_rect(
      gfx::RectF(rect), upper_left_radius, upper_left_radius,
      upper_right_radius, upper_right_radius, lower_right_radius,
      lower_right_radius, lower_left_radius, lower_left_radius);

  canvas->drawRRect(static_cast<SkRRect>(rounded_rect), flags);
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
