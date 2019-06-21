// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_typography_provider.h"

#include "chrome/browser/ui/views/chrome_typography.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/platform_font.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "ui/native_theme/native_theme_win.h"
#endif

#if defined(OS_CHROMEOS)
// gn check complains on Linux Ozone.
#include "ash/public/cpp/ash_typography.h"  // nogncheck
#endif

namespace {

#if defined(OS_MACOSX)
constexpr char kDefaultMonospacedTypeface[] = "Menlo";
#elif defined(OS_WIN)
constexpr char kDefaultMonospacedTypeface[] = "Consolas";
#else
constexpr char kDefaultMonospacedTypeface[] = "DejaVu Sans Mono";
#endif
constexpr char kUnspecifiedTypeface[] = "";

// If the default foreground color from the native theme isn't black and dark
// mode is not on the rest of the Harmony spec isn't going to work. Also skip
// Harmony if a Windows High Contrast theme is enabled. One of the four standard
// High Contrast themes in Windows 10 still has black text, but (since the user
// wants high contrast) the grey text shades in Harmony should not be used.
bool ShouldIgnoreHarmonySpec(const ui::NativeTheme& theme) {
  // Mac provides users limited ways to customize the UI, including dark and
  // high contrast modes; all these are addressed elsewhere, so there's no need
  // for Mac to try to detect non-Harmony cases as Windows and Linux need to,
  // and dark mode can interfere with the detection below.
#if defined(OS_MACOSX)
  return false;
#else
  if (theme.UsesHighContrastColors())
    return true;
  if (theme.SystemDarkModeEnabled())
    return false;

  // TODO(pbos): Revisit this check. Both GG900 and black are considered
  // "default black" as the common theme uses GG900 as primary color.
  const SkColor test_color =
      theme.GetSystemColor(ui::NativeTheme::kColorId_LabelEnabledColor);
  const bool label_color_is_black =
      test_color == SK_ColorBLACK || test_color == gfx::kGoogleGrey900;
  return !label_color_is_black;
#endif  // defined(OS_MACOSX)
}

// Returns a color for a possibly inverted or high-contrast OS color theme.
SkColor GetHarmonyTextColorForNonStandardNativeTheme(
    int context,
    int style,
    const ui::NativeTheme& theme) {
  // At the time of writing, very few UI surfaces need typography for a Chrome-
  // provided theme. Typically just incognito browser windows (when the native
  // theme is NativeThemeDarkAura). Instead, this method is consulted when the
  // actual OS theme is configured in a special way. So pick from a small number
  // of NativeTheme constants that are known to adapt properly to distinct
  // colors when configuring the OS to use a high-contrast theme. For example,
  // ::GetSysColor() on Windows has 8 text colors: BTNTEXT, CAPTIONTEXT,
  // GRAYTEXT, HIGHLIGHTTEXT, INACTIVECAPTIONTEXT, INFOTEXT (tool tips),
  // MENUTEXT, and WINDOWTEXT. There's also hyperlinks: COLOR_HOTLIGHT.
  // Diverging from these risks using a color that doesn't match user
  // expectations.

  const bool inverted_scheme = color_utils::IsInvertedColorScheme();

  ui::NativeTheme::ColorId color_id =
      (context == views::style::CONTEXT_BUTTON ||
       context == views::style::CONTEXT_BUTTON_MD)
          ? ui::NativeTheme::kColorId_ButtonEnabledColor
          : ui::NativeTheme::kColorId_TextfieldDefaultColor;
  switch (style) {
    case views::style::STYLE_DIALOG_BUTTON_DEFAULT:
      // This is just white in Harmony and, even in inverted themes, prominent
      // buttons have a dark background, so white will maximize contrast.
      return SK_ColorWHITE;
    case views::style::STYLE_DISABLED:
      color_id = ui::NativeTheme::kColorId_LabelDisabledColor;
      break;
    case views::style::STYLE_LINK:
      color_id = ui::NativeTheme::kColorId_LinkEnabled;
      break;
    case STYLE_RED:
      return inverted_scheme ? gfx::kGoogleRed300 : gfx::kGoogleRed600;
    case STYLE_GREEN:
      return inverted_scheme ? gfx::kGoogleGreen300 : gfx::kGoogleGreen600;
  }
  return theme.GetSystemColor(color_id);
}

}  // namespace

const gfx::FontList& ChromeTypographyProvider::GetFont(int context,
                                                       int style) const {
  // "Target" font size constants.
  constexpr int kHeadlineSize = 20;
  constexpr int kTitleSize = 15;
  constexpr int kTouchableLabelSize = 14;
  constexpr int kBodyTextLargeSize = 13;
  constexpr int kDefaultSize = 12;

  std::string typeface = kUnspecifiedTypeface;
  int size_delta = kDefaultSize - gfx::PlatformFont::kDefaultBaseFontSize;
  gfx::Font::Weight font_weight = gfx::Font::Weight::NORMAL;

#if defined(OS_CHROMEOS)
  ash::ApplyAshFontStyles(context, style, &size_delta, &font_weight);
#endif

  ApplyCommonFontStyles(context, style, &size_delta, &font_weight);

  switch (context) {
    case views::style::CONTEXT_BUTTON_MD:
      font_weight = MediumWeightForUI();
      break;
    case views::style::CONTEXT_DIALOG_TITLE:
      size_delta = kTitleSize - gfx::PlatformFont::kDefaultBaseFontSize;
      break;
    case views::style::CONTEXT_TOUCH_MENU:
      size_delta =
          kTouchableLabelSize - gfx::PlatformFont::kDefaultBaseFontSize;
      break;
    case CONTEXT_BODY_TEXT_LARGE:
    case CONTEXT_TAB_HOVER_CARD_TITLE:
    case views::style::CONTEXT_MESSAGE_BOX_BODY_TEXT:
      size_delta = kBodyTextLargeSize - gfx::PlatformFont::kDefaultBaseFontSize;
      break;
    case CONTEXT_HEADLINE:
      size_delta = kHeadlineSize - gfx::PlatformFont::kDefaultBaseFontSize;
      break;
    default:
      break;
  }

  if (context == CONTEXT_TAB_HOVER_CARD_TITLE) {
    DCHECK_EQ(views::style::STYLE_PRIMARY, style);
    font_weight = gfx::Font::Weight::SEMIBOLD;
  }

  // Use a bold style for emphasized text in body contexts, and ignore |style|
  // otherwise.
  if (style == STYLE_EMPHASIZED || style == STYLE_EMPHASIZED_SECONDARY) {
    switch (context) {
      case CONTEXT_BODY_TEXT_SMALL:
      case CONTEXT_BODY_TEXT_LARGE:
      case views::style::CONTEXT_MESSAGE_BOX_BODY_TEXT:
        font_weight = gfx::Font::Weight::BOLD;
        break;

      default:
        break;
    }
  }

  if (style == STYLE_PRIMARY_MONOSPACED ||
      style == STYLE_SECONDARY_MONOSPACED) {
    typeface = kDefaultMonospacedTypeface;
  }

  return ui::ResourceBundle::GetSharedInstance()
      .GetFontListWithTypefaceAndDelta(typeface, size_delta, gfx::Font::NORMAL,
                                       font_weight);
}

SkColor ChromeTypographyProvider::GetColor(const views::View& view,
                                           int context,
                                           int style) const {
  // TODO(lgrey): Remove anything that could be using native theme
  // colors from here after UX review of divergences.
  const ui::NativeTheme* native_theme = view.GetNativeTheme();
  DCHECK(native_theme);
  if (ShouldIgnoreHarmonySpec(*native_theme)) {
    return GetHarmonyTextColorForNonStandardNativeTheme(context, style,
                                                        *native_theme);
  }

  if (context == views::style::CONTEXT_BUTTON_MD) {
    switch (style) {
      case views::style::STYLE_DIALOG_BUTTON_DEFAULT:
        return native_theme->SystemDarkModeEnabled() ? gfx::kGoogleGrey900
                                                     : SK_ColorWHITE;
      case views::style::STYLE_DISABLED:
        return gfx::kGoogleGrey600;
      default:
        return native_theme->SystemDarkModeEnabled() ? gfx::kGoogleBlue300
                                                     : gfx::kGoogleBlue600;
    }
  }

  // Use the secondary style instead of primary for message box body text.
  if (context == views::style::CONTEXT_MESSAGE_BOX_BODY_TEXT) {
    if (style == views::style::STYLE_PRIMARY) {
      style = STYLE_SECONDARY;
    } else if (style == STYLE_PRIMARY_MONOSPACED) {
      style = STYLE_SECONDARY_MONOSPACED;
    }
  }

  switch (style) {
    case views::style::STYLE_DIALOG_BUTTON_DEFAULT:
      return SK_ColorWHITE;
    case views::style::STYLE_DISABLED:
      return native_theme->SystemDarkModeEnabled()
                 ? gfx::kGoogleGrey800
                 : SkColorSetRGB(0x9e, 0x9e, 0x9e);
    case views::style::STYLE_LINK:
      return gfx::kGoogleBlue700;
    case STYLE_SECONDARY:
    case STYLE_SECONDARY_MONOSPACED:
    case STYLE_EMPHASIZED_SECONDARY:
    case STYLE_HINT:
      return native_theme->SystemDarkModeEnabled() ? gfx::kGoogleGrey500
                                                   : gfx::kGoogleGrey700;
    case STYLE_RED:
      return native_theme->SystemDarkModeEnabled() ? gfx::kGoogleRed300
                                                   : gfx::kGoogleRed700;
    case STYLE_GREEN:
      return native_theme->SystemDarkModeEnabled() ? gfx::kGoogleGreen300
                                                   : gfx::kGoogleGreen700;
  }

  // Use default primary color for everything else.
  return native_theme->SystemDarkModeEnabled()
             ? SkColorSetA(SK_ColorWHITE, 0xDD)
             : gfx::kGoogleGrey900;
}

int ChromeTypographyProvider::GetLineHeight(int context, int style) const {
  // "Target" line height constants from the Harmony spec. A default OS
  // configuration should use these heights. However, if the user overrides OS
  // defaults, then GetLineHeight() should return the height that would add the
  // same extra space between lines as the default configuration would have.
  constexpr int kHeadlineHeight = 32;
  constexpr int kTitleHeight = 22;
  constexpr int kBodyHeight = 20;  // For both large and small.

  // Button text should always use the minimum line height for a font to avoid
  // unnecessarily influencing the height of a button.
  constexpr int kButtonAbsoluteHeight = 0;

// The platform-specific heights (i.e. gfx::Font::GetHeight()) that result when
// asking for the target size constants in ChromeTypographyProvider::GetFont()
// in a default OS configuration.
#if defined(OS_MACOSX)
  constexpr int kHeadlinePlatformHeight = 25;
  constexpr int kTitlePlatformHeight = 19;
  constexpr int kBodyTextLargePlatformHeight = 16;
  constexpr int kBodyTextSmallPlatformHeight = 15;
#elif defined(OS_WIN)
  constexpr int kHeadlinePlatformHeight = 27;
  constexpr int kTitlePlatformHeight = 20;
  constexpr int kBodyTextLargePlatformHeight = 18;
  constexpr int kBodyTextSmallPlatformHeight = 16;
#else
  constexpr int kHeadlinePlatformHeight = 24;
  constexpr int kTitlePlatformHeight = 18;
  constexpr int kBodyTextLargePlatformHeight = 17;
  constexpr int kBodyTextSmallPlatformHeight = 15;
#endif

  // The style of the system font used to determine line heights.
  constexpr int kTemplateStyle = views::style::STYLE_PRIMARY;

  // TODO(tapted): These statics should be cleared out when something invokes
  // ui::ResourceBundle::ReloadFonts(). Currently that only happens on ChromeOS.
  // See http://crbug.com/708943.
  static const int headline_height =
      GetFont(CONTEXT_HEADLINE, kTemplateStyle).GetHeight() -
      kHeadlinePlatformHeight + kHeadlineHeight;
  static const int title_height =
      GetFont(views::style::CONTEXT_DIALOG_TITLE, kTemplateStyle).GetHeight() -
      kTitlePlatformHeight + kTitleHeight;
  static const int body_large_height =
      GetFont(CONTEXT_BODY_TEXT_LARGE, kTemplateStyle).GetHeight() -
      kBodyTextLargePlatformHeight + kBodyHeight;
  static const int default_height =
      GetFont(CONTEXT_BODY_TEXT_SMALL, kTemplateStyle).GetHeight() -
      kBodyTextSmallPlatformHeight + kBodyHeight;

  switch (context) {
    case views::style::CONTEXT_BUTTON:
    case views::style::CONTEXT_BUTTON_MD:
    case CONTEXT_TOOLBAR_BUTTON:
      return kButtonAbsoluteHeight;
    case views::style::CONTEXT_DIALOG_TITLE:
      return title_height;
    case CONTEXT_BODY_TEXT_LARGE:
    case CONTEXT_TAB_HOVER_CARD_TITLE:
    case views::style::CONTEXT_MESSAGE_BOX_BODY_TEXT:
    case views::style::CONTEXT_TABLE_ROW:
      return body_large_height;
    case CONTEXT_HEADLINE:
      return headline_height;
    default:
      return default_height;
  }
}
