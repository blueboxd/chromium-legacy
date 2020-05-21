// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_mac.h"

#import <Cocoa/Cocoa.h>
#include <stddef.h>
#include <vector>

#include "base/command_line.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_block.h"
#include "base/macros.h"
#import "skia/ext/skia_utils_mac.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/color/mac/scoped_current_nsappearance.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/skia_util.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme_aura.h"

namespace {

// TODO: crbug.com/1082918 - Clean up these hard coded colors and get
// them behind ids and functions like GetOSColor.
// The width of the scroller track border.
constexpr int kTrackBorderWidth = 1;

// The amount the thumb is inset from the ends and the inside edge of track
// border.
constexpr int kThumbInset = 3;
constexpr int kThumbInsetOverlay = 2;

// The minimum sizes for the thumb. We will not inset the thumb if it will
// be smaller than this size.
constexpr int kThumbMinGirth = 6;
constexpr int kThumbMinLength = 18;

// Scrollbar thumb colors.
constexpr SkColor kThumbColorDefault = SkColorSetARGB(0x3A, 0, 0, 0);
constexpr SkColor kThumbColorHover = SkColorSetARGB(0x80, 0, 0, 0);
constexpr SkColor kThumbColorDarkMode = SkColorSetRGB(0x6B, 0x6B, 0x6B);
constexpr SkColor kThumbColorDarkModeHover = SkColorSetRGB(0x93, 0x93, 0x93);
constexpr SkColor kThumbColorOverlay = SkColorSetARGB(0x80, 0, 0, 0);
constexpr SkColor kThumbColorOverlayDarkMode =
    SkColorSetARGB(0x80, 0xFF, 0xFF, 0xFF);

constexpr SkColor kTrackInnerBorderColor = SkColorSetRGB(0xE8, 0xE8, 0xE8);
constexpr SkColor kTrackOuterBorderColor = SkColorSetRGB(0xED, 0xED, 0xED);

constexpr SkColor kTrackInnerBorderColorDarkMode =
    SkColorSetRGB(0x3D, 0x3D, 0x3D);
constexpr SkColor kTrackOuterBorderColorDarkMode =
    SkColorSetRGB(0x51, 0x51, 0x51);

constexpr SkColor kTrackInnerBorderColorOverlay =
    SkColorSetARGB(0xF9, 0xDF, 0xDF, 0xDF);
constexpr SkColor kTrackOuterBorderColorOverlay =
    SkColorSetARGB(0xC6, 0xE8, 0xE8, 0xE8);

// Dark mode overlay scroller track colors.
constexpr SkColor kTrackInnerBorderColorOverlayDarkMode =
    SkColorSetARGB(0x33, 0xE5, 0xE5, 0xE5);
constexpr SkColor kTrackOuterBorderColorOverlayDarkMode =
    SkColorSetARGB(0x28, 0xD8, 0xD8, 0xD8);

bool IsDarkMode() {
  if (@available(macOS 10.14, *)) {
    NSAppearanceName appearance =
        [[NSApp effectiveAppearance] bestMatchFromAppearancesWithNames:@[
          NSAppearanceNameAqua, NSAppearanceNameDarkAqua
        ]];
    return [appearance isEqual:NSAppearanceNameDarkAqua];
  }
  return false;
}

bool IsHighContrast() {
  NSWorkspace* workspace = [NSWorkspace sharedWorkspace];
  if ([workspace respondsToSelector:@selector
                 (accessibilityDisplayShouldIncreaseContrast)]) {
    return workspace.accessibilityDisplayShouldIncreaseContrast;
  }
  return false;
}
}  // namespace

@interface NSWorkspace (Redeclarations)

@property(readonly) BOOL accessibilityDisplayShouldIncreaseContrast;

@end

// Helper object to respond to light mode/dark mode changeovers.
@interface NativeThemeEffectiveAppearanceObserver : NSObject
@end

@implementation NativeThemeEffectiveAppearanceObserver {
  base::mac::ScopedBlock<void (^)()> _handler;
}

- (instancetype)initWithHandler:(void (^)())handler {
  self = [super init];
  if (self) {
    _handler.reset([handler copy]);
    if (@available(macOS 10.14, *)) {
      [NSApp addObserver:self
              forKeyPath:@"effectiveAppearance"
                 options:0
                 context:nullptr];
    }
  }
  return self;
}

- (void)dealloc {
  if (@available(macOS 10.14, *)) {
    [NSApp removeObserver:self forKeyPath:@"effectiveAppearance"];
  }
  [super dealloc];
}

- (void)observeValueForKeyPath:(NSString*)forKeyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  _handler.get()();
}

@end

namespace {

// Helper to make indexing an array by an enum class easier.
template <class KEY, class VALUE>
struct EnumArray {
  VALUE& operator[](const KEY& key) { return array[static_cast<size_t>(key)]; }
  VALUE array[static_cast<size_t>(KEY::COUNT)];
};

// Converts an SkColor to grayscale by using luminance for all three components.
// Experimentally, this seems to produce a better result than a flat average or
// a min/max average for UI controls.
SkColor ColorToGrayscale(SkColor color) {
  SkScalar luminance = SkColorGetR(color) * 0.21 +
                       SkColorGetG(color) * 0.72 +
                       SkColorGetB(color) * 0.07;
  uint8_t component = SkScalarRoundToInt(luminance);
  return SkColorSetARGB(SkColorGetA(color), component, component, component);
}

}  // namespace

namespace ui {

// static
NativeTheme* NativeTheme::GetInstanceForWeb() {
  if (features::IsFormControlsRefreshEnabled())
    return NativeThemeAura::web_instance();
  return NativeThemeMac::instance();
}

// static
NativeTheme* NativeTheme::GetInstanceForNativeUi() {
  return NativeThemeMac::instance();
}

NativeTheme* NativeTheme::GetInstanceForDarkUI() {
  static base::NoDestructor<NativeThemeMac> s_native_theme(false, true);
  return s_native_theme.get();
}

// static
bool NativeTheme::SystemDarkModeSupported() {
  if (@available(macOS 10.14, *)) {
    return true;
  }
  return false;
}

// static
NativeThemeMac* NativeThemeMac::instance() {
  static base::NoDestructor<NativeThemeMac> s_native_theme(true, false);
  return s_native_theme.get();
}

// static
SkColor NativeThemeMac::ApplySystemControlTint(SkColor color) {
  if ([NSColor currentControlTint] == NSGraphiteControlTint)
    return ColorToGrayscale(color);
  return color;
}

SkColor NativeThemeMac::GetSystemColor(ColorId color_id,
                                       ColorScheme color_scheme) const {
  if (color_scheme == ColorScheme::kDefault)
    color_scheme = GetDefaultSystemColorScheme();

  // The first check makes sure that when we are using the color providers that
  // we actually go to the providers instead of just returning the  colors
  // below. The second check is to make sure that when not using color
  // providers, we only skip the rest of the method when we are in an incognito
  // window.
  // TODO(http://crbug.com/1057754): Remove the && kPlatformHighContrast
  // once NativeTheme.cc handles kColorProviderReirection and
  // kPlatformHighContrast both being on.
  if ((base::FeatureList::IsEnabled(features::kColorProviderRedirection) &&
       color_scheme != ColorScheme::kPlatformHighContrast) ||
      should_only_use_dark_colors_)
    return NativeTheme::GetSystemColor(color_id, color_scheme);

  if (UsesHighContrastColors()) {
    switch (color_id) {
      case kColorId_SelectedMenuItemForegroundColor:
        return color_scheme == ColorScheme::kDark ? SK_ColorBLACK
                                                  : SK_ColorWHITE;
      case kColorId_FocusedMenuItemBackgroundColor:
        return color_scheme == ColorScheme::kDark ? SK_ColorLTGRAY
                                                  : SK_ColorDKGRAY;
      default:
        break;
    }
  }

  base::Optional<SkColor> os_color = GetOSColor(color_id, color_scheme);
  if (os_color.has_value())
    return os_color.value();

  return ApplySystemControlTint(
      NativeTheme::GetSystemColor(color_id, color_scheme));
}

base::Optional<SkColor> NativeThemeMac::GetOSColor(
    ColorId color_id,
    ColorScheme color_scheme) const {
  ScopedCurrentNSAppearance scoped_nsappearance(color_scheme ==
                                                ColorScheme::kDark);

  // Even with --secondary-ui-md, menus use the platform colors and styling, and
  // Mac has a couple of specific color overrides, documented below.
  switch (color_id) {
    case kColorId_EnabledMenuItemForegroundColor:
      return skia::NSSystemColorToSkColor([NSColor controlTextColor]);
    case kColorId_DisabledMenuItemForegroundColor:
      return skia::NSSystemColorToSkColor([NSColor disabledControlTextColor]);
    case kColorId_MenuSeparatorColor:
      return color_scheme == ColorScheme::kDark
                 ? SkColorSetA(gfx::kGoogleGrey800, 0xCC)
                 : SkColorSetA(SK_ColorBLACK, 0x26);
    case kColorId_MenuBorderColor:
      return SkColorSetA(SK_ColorBLACK, 0x60);

    // There's a system setting General > Highlight color which sets the
    // background color for text selections. We honor that setting.
    // TODO(ellyjones): Listen for NSSystemColorsDidChangeNotification somewhere
    // and propagate it to the View hierarchy.
    case kColorId_LabelTextSelectionBackgroundFocused:
    case kColorId_TextfieldSelectionBackgroundFocused:
      return skia::NSSystemColorToSkColor(
          [NSColor selectedTextBackgroundColor]);

    case kColorId_FocusedBorderColor:
      return SkColorSetA(
          skia::NSSystemColorToSkColor([NSColor keyboardFocusIndicatorColor]),
          0x66);

    case kColorId_TableBackgroundAlternate:
      if (@available(macOS 10.14, *)) {
        return skia::NSSystemColorToSkColor(
            NSColor.alternatingContentBackgroundColors[1]);
      }
      return skia::NSSystemColorToSkColor(
          NSColor.controlAlternatingRowBackgroundColors[1]);

    default:
      return base::nullopt;
  }
}

void NativeThemeMac::Paint(cc::PaintCanvas* canvas,
                           Part part,
                           State state,
                           const gfx::Rect& rect,
                           const ExtraParams& extra,
                           ColorScheme color_scheme) const {
  ColorScheme color_scheme_updated = color_scheme;
  if (color_scheme_updated == ColorScheme::kDefault)
    color_scheme_updated = GetDefaultSystemColorScheme();

  if (rect.IsEmpty())
    return;

  switch (part) {
    case kScrollbarHorizontalThumb:
    case kScrollbarVerticalThumb:
      PaintMacScrollbarThumb(canvas, part, state, rect, extra.scrollbar_extra,
                             color_scheme_updated);
      break;
    case kScrollbarHorizontalTrack:
    case kScrollbarVerticalTrack:
      PaintMacScrollBarTrackOrCorner(canvas, part, state, extra.scrollbar_extra,
                                     rect, color_scheme_updated, false);
      break;
    case kScrollbarCorner:
      PaintMacScrollBarTrackOrCorner(canvas, part, state, extra.scrollbar_extra,
                                     rect, color_scheme_updated, true);
      break;
    default:
      NativeThemeBase::Paint(canvas, part, state, rect, extra, color_scheme);
      break;
  }
}

void ConstrainInsets(int old_width, int min_width, int* left, int* right) {
  int requested_total_inset = *left + *right;
  if (requested_total_inset == 0)
    return;
  int max_total_inset = old_width - min_width;
  if (requested_total_inset < max_total_inset)
    return;
  if (max_total_inset < 0) {
    *left = *right = 0;
    return;
  }
  // Multiply the right/bottom inset by the ratio by which we need to shrink the
  // total inset. This has the effect of rounding down the right/bottom inset,
  // if the two sides are to be affected unevenly.
  // This is done instead of using inset scale functions to maintain expected
  // behavior and to map to how it looks like other scrollbars work on MacOS.
  *right *= max_total_inset * 1.0f / requested_total_inset;
  *left = max_total_inset - *right;
}

void ConstrainedInset(gfx::Rect* rect,
                      gfx::Size min_size,
                      gfx::Insets initial_insets) {
  int inset_left = initial_insets.left();
  int inset_right = initial_insets.right();
  int inset_top = initial_insets.top();
  int inset_bottom = initial_insets.bottom();

  ConstrainInsets(rect->width(), min_size.width(), &inset_left, &inset_right);
  ConstrainInsets(rect->height(), min_size.height(), &inset_top, &inset_bottom);
  rect->Inset(inset_left, inset_top, inset_right, inset_bottom);
}

void NativeThemeMac::PaintMacScrollBarTrackOrCorner(
    cc::PaintCanvas* canvas,
    Part part,
    State state,
    const ScrollbarExtraParams& extra_params,
    const gfx::Rect& rect,
    ColorScheme color_scheme,
    bool is_corner) const {
  if (is_corner && extra_params.is_overlay)
    return;
  PaintScrollBarTrackGradient(canvas, rect, extra_params, is_corner,
                              color_scheme);
  PaintScrollbarTrackInnerBorder(canvas, rect, extra_params, is_corner,
                                 color_scheme);
  PaintScrollbarTrackOuterBorder(canvas, rect, extra_params, is_corner,
                                 color_scheme);
}

void NativeThemeMac::PaintScrollBarTrackGradient(
    cc::PaintCanvas* canvas,
    const gfx::Rect& rect,
    const ScrollbarExtraParams& extra_params,
    bool is_corner,
    ColorScheme color_scheme) const {
  gfx::Canvas paint_canvas(canvas, 1.0f);
  // Select colors.
  std::vector<SkColor> gradient_colors;
  bool dark_mode = color_scheme == ColorScheme::kDark;
  if (extra_params.is_overlay) {
    if (dark_mode) {
      gradient_colors = {SkColorSetARGB(0x28, 0xD8, 0xD8, 0xD8),
                         SkColorSetARGB(0x26, 0xCC, 0xCC, 0xCC),
                         SkColorSetARGB(0x26, 0xCC, 0xCC, 0xCC),
                         SkColorSetARGB(0x26, 0xCC, 0xCC, 0xCC)};
    } else {
      gradient_colors = {SkColorSetARGB(0xC6, 0xF8, 0xF8, 0xF8),
                         SkColorSetARGB(0xC2, 0xF8, 0xF8, 0xF8),
                         SkColorSetARGB(0xC2, 0xF8, 0xF8, 0xF8),
                         SkColorSetARGB(0xC2, 0xF8, 0xF8, 0xF8)};
    }
  } else {
    // Non-overlay scroller track colors are not transparent. On Safari, they
    // are, but on all other macOS applications they are not.
    if (dark_mode) {
      gradient_colors = {SkColorSetRGB(0x2D, 0x2D, 0x2D),
                         SkColorSetRGB(0x2B, 0x2B, 0x2B)};
    } else {
      gradient_colors = {SkColorSetRGB(0xFA, 0xFA, 0xFA),
                         SkColorSetRGB(0xFA, 0xFA, 0xFA)};
    }
  }

  // Set the gradient direction.
  std::vector<SkPoint> gradient_bounds;
  if (is_corner) {
    if (extra_params.orientation == ScrollbarOrientation::kVerticalOnRight) {
      gradient_bounds = {gfx::PointToSkPoint(rect.origin()),
                         gfx::PointToSkPoint(rect.bottom_right())};
    } else {
      gradient_bounds = {gfx::PointToSkPoint(rect.top_right()),
                         gfx::PointToSkPoint(rect.bottom_left())};
    }
  } else {
    if (extra_params.orientation == ScrollbarOrientation::kHorizontal) {
      gradient_bounds = {gfx::PointToSkPoint(rect.origin()),
                         gfx::PointToSkPoint(rect.top_right())};
    } else {
      gradient_bounds = {gfx::PointToSkPoint(rect.origin()),
                         gfx::PointToSkPoint(rect.bottom_left())};
    }
  }

  // And draw.
  cc::PaintFlags gradient;
  gradient.setShader(cc::PaintShader::MakeLinearGradient(
      gradient_bounds.data(), gradient_colors.data(), nullptr,
      gradient_colors.size(), SkTileMode::kClamp));
  paint_canvas.DrawRect(rect, gradient);
}

void NativeThemeMac::PaintScrollbarTrackInnerBorder(
    cc::PaintCanvas* canvas,
    const gfx::Rect& rect,
    const ScrollbarExtraParams& extra_params,
    bool is_corner,
    ColorScheme color_scheme) const {
  gfx::Canvas paint_canvas(canvas, 1.0f);
  // Select the color.
  bool dark_mode = color_scheme == ColorScheme::kDark;
  SkColor inner_border_color = 0;
  if (extra_params.is_overlay) {
    inner_border_color = dark_mode ? kTrackInnerBorderColorOverlayDarkMode
                                   : kTrackInnerBorderColorOverlay;
  } else {
    inner_border_color =
        dark_mode ? kTrackInnerBorderColorDarkMode : kTrackInnerBorderColor;
  }

  // Compute the rect for the border.
  gfx::Rect inner_border(rect);
  if (extra_params.orientation == ScrollbarOrientation::kVerticalOnLeft)
    inner_border.set_x(rect.right() - kTrackBorderWidth);
  if (is_corner ||
      extra_params.orientation == ScrollbarOrientation::kHorizontal)
    inner_border.set_height(kTrackBorderWidth);
  if (is_corner ||
      extra_params.orientation != ScrollbarOrientation::kHorizontal)
    inner_border.set_width(kTrackBorderWidth);

  // And draw.
  cc::PaintFlags flags;
  flags.setColor(inner_border_color);
  paint_canvas.DrawRect(inner_border, flags);
}

void NativeThemeMac::PaintScrollbarTrackOuterBorder(
    cc::PaintCanvas* canvas,
    const gfx::Rect& rect,
    const ScrollbarExtraParams& extra_params,
    bool is_corner,
    ColorScheme color_scheme) const {
  gfx::Canvas paint_canvas(canvas, 1.0f);
  // Select the color.
  SkColor outer_border_color = 0;
  bool dark_mode = color_scheme == ColorScheme::kDark;
  if (extra_params.is_overlay) {
    outer_border_color = dark_mode ? kTrackOuterBorderColorOverlayDarkMode
                                   : kTrackOuterBorderColorOverlay;
  } else {
    outer_border_color =
        dark_mode ? kTrackOuterBorderColorDarkMode : kTrackOuterBorderColor;
  }
  cc::PaintFlags flags;
  flags.setColor(outer_border_color);

  // Draw the horizontal outer border.
  if (is_corner ||
      extra_params.orientation == ScrollbarOrientation::kHorizontal) {
    gfx::Rect outer_border(rect);
    outer_border.set_height(kTrackBorderWidth);
    outer_border.set_y(rect.bottom() - kTrackBorderWidth);
    paint_canvas.DrawRect(outer_border, flags);
  }

  // Draw the vertial outer border.
  if (is_corner ||
      extra_params.orientation != ScrollbarOrientation::kHorizontal) {
    gfx::Rect outer_border(rect);
    outer_border.set_width(kTrackBorderWidth);
    if (extra_params.orientation == ScrollbarOrientation::kVerticalOnRight)
      outer_border.set_x(rect.right() - kTrackBorderWidth);
    paint_canvas.DrawRect(outer_border, flags);
  }
}

void NativeThemeMac::PaintMacScrollbarThumb(
    cc::PaintCanvas* canvas,
    Part part,
    State state,
    const gfx::Rect& rect,
    const ScrollbarExtraParams& scroll_thumb,
    ColorScheme color_scheme) const {
  gfx::Canvas paint_canvas(canvas, 1.0f);

  // Select the color.
  SkColor thumb_color = 0;
  bool dark_mode = color_scheme == ColorScheme::kDark;
  if (scroll_thumb.is_overlay) {
    thumb_color = dark_mode ? kThumbColorOverlayDarkMode : kThumbColorOverlay;
  } else {
    if (dark_mode) {
      thumb_color = scroll_thumb.is_hovering ? kThumbColorDarkModeHover
                                             : kThumbColorDarkMode;
    } else {
      thumb_color =
          scroll_thumb.is_hovering ? kThumbColorHover : kThumbColorDefault;
    }
  }

  // Compute the bounds for the rounded rect for the thumb from the bounds of
  // the thumb.
  gfx::Rect bounds(rect);
  {
    // Shrink the thumb evenly in length and girth to fit within the track.
    const int thumb_inset =
        scroll_thumb.is_overlay ? kThumbInsetOverlay : kThumbInset;
    gfx::Insets thumb_insets(thumb_inset);

    // Also shrink the thumb in girth to not touch the border.
    if (scroll_thumb.orientation == ScrollbarOrientation::kHorizontal) {
      thumb_insets.set_top(thumb_insets.top() + kTrackBorderWidth);
      ConstrainedInset(&bounds, gfx::Size(kThumbMinLength, kThumbMinGirth),
                       thumb_insets);
    } else {
      thumb_insets.set_left(thumb_insets.left() + kTrackBorderWidth);
      ConstrainedInset(&bounds, gfx::Size(kThumbMinGirth, kThumbMinLength),
                       thumb_insets);
    }
  }

  // Draw.
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(thumb_color);
  const SkScalar radius = std::min(bounds.width(), bounds.height());
  paint_canvas.DrawRoundRect(bounds, radius, flags);
}

SkColor NativeThemeMac::GetSystemButtonPressedColor(SkColor base_color) const {
  // TODO crbug.com/1003612: This should probably be replaced with a color
  // transform.
  // Mac has a different "pressed button" styling because it doesn't use
  // ripples.
  return color_utils::GetResultingPaintColor(SkColorSetA(SK_ColorBLACK, 0x10),
                                             base_color);
}

void NativeThemeMac::PaintMenuPopupBackground(
    cc::PaintCanvas* canvas,
    const gfx::Size& size,
    const MenuBackgroundExtraParams& menu_background,
    ColorScheme color_scheme) const {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(GetSystemColor(kColorId_MenuBackgroundColor, color_scheme));
  const SkScalar radius = SkIntToScalar(menu_background.corner_radius);
  SkRect rect = gfx::RectToSkRect(gfx::Rect(size));
  canvas->drawRoundRect(rect, radius, radius, flags);
}

void NativeThemeMac::PaintMenuItemBackground(
    cc::PaintCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const MenuItemExtraParams& menu_item,
    ColorScheme color_scheme) const {
  switch (state) {
    case NativeTheme::kNormal:
    case NativeTheme::kDisabled:
      // Draw nothing over the regular background.
      break;
    case NativeTheme::kHovered:
      PaintSelectedMenuItem(canvas, rect, color_scheme);
      break;
    default:
      NOTREACHED();
      break;
  }
}

NativeThemeMac::NativeThemeMac(bool configure_web_instance,
                               bool should_only_use_dark_colors)
    : NativeThemeBase(should_only_use_dark_colors),
      should_only_use_dark_colors_(should_only_use_dark_colors) {
  if (!should_only_use_dark_colors)
    InitializeDarkModeStateAndObserver();

  if (!IsForcedHighContrast()) {
    set_high_contrast(IsHighContrast());
    __block auto theme = this;
  }

  if (configure_web_instance)
    ConfigureWebInstance();
}

NativeThemeMac::~NativeThemeMac() {
  [[NSNotificationCenter defaultCenter]
      removeObserver:high_contrast_notification_token_];
}

void NativeThemeMac::PaintSelectedMenuItem(cc::PaintCanvas* canvas,
                                           const gfx::Rect& rect,
                                           ColorScheme color_scheme) const {
  // Draw the background.
  cc::PaintFlags flags;
  flags.setColor(
      GetSystemColor(kColorId_FocusedMenuItemBackgroundColor, color_scheme));
  canvas->drawRect(gfx::RectToSkRect(rect), flags);
}

void NativeThemeMac::InitializeDarkModeStateAndObserver() {
  __block auto theme = this;
  set_use_dark_colors(IsDarkMode());
  set_preferred_color_scheme(CalculatePreferredColorScheme());
  appearance_observer_.reset(
      [[NativeThemeEffectiveAppearanceObserver alloc] initWithHandler:^{
        theme->set_use_dark_colors(IsDarkMode());
        theme->set_preferred_color_scheme(CalculatePreferredColorScheme());
        theme->NotifyObservers();
      }]);
}

void NativeThemeMac::ConfigureWebInstance() {
  if (!features::IsFormControlsRefreshEnabled())
    return;

  // For FormControlsRefresh, NativeThemeAura is used as web instance so we need
  // to initialize its state.
  NativeTheme* web_instance = NativeTheme::GetInstanceForWeb();
  web_instance->set_use_dark_colors(IsDarkMode());
  web_instance->set_preferred_color_scheme(CalculatePreferredColorScheme());
  web_instance->set_high_contrast(IsHighContrast());

  // Add the web native theme as an observer to stay in sync with dark mode,
  // high contrast, and preferred color scheme changes.
  color_scheme_observer_ =
      std::make_unique<NativeTheme::ColorSchemeNativeThemeObserver>(
          NativeTheme::GetInstanceForWeb());
  AddObserver(color_scheme_observer_.get());
}

}  // namespace ui
