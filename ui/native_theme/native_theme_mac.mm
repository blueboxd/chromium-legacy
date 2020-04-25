// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_mac.h"

#import <Cocoa/Cocoa.h>
#include <stddef.h>

#include "base/command_line.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_block.h"
#include "base/macros.h"
#import "skia/ext/skia_utils_mac.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/color/mac/scoped_current_nsappearance.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/skia_util.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme_aura.h"

namespace {

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

    // Mac has a different "pressed button" styling because it doesn't use
    // ripples.
    case kColorId_ButtonPressedShade:
      return SkColorSetA(SK_ColorBLACK, 0x10);

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
//    high_contrast_notification_token_ =
//        [[[NSWorkspace sharedWorkspace] notificationCenter]
//            addObserverForName:
//                NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification
//                        object:nil
//                         queue:nil
//                    usingBlock:^(NSNotification* notification) {
//                      theme->set_high_contrast(IsHighContrast());
//                      theme->NotifyObservers();
//                    }];
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
