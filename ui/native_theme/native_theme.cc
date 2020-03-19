// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme.h"

#include <cstring>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
#include "ui/native_theme/common_theme.h"

#if !defined(OS_ANDROID)
#include "ui/color/color_mixers.h"
#endif

namespace ui {

NativeTheme::ExtraParams::ExtraParams() {
  memset(this, 0, sizeof(*this));
}

NativeTheme::ExtraParams::ExtraParams(const ExtraParams& other) {
  memcpy(this, &other, sizeof(*this));
}

#if !defined(OS_WIN) && !defined(OS_MACOSX)
// static
bool NativeTheme::SystemDarkModeSupported() {
  return false;
}
#endif

SkColor NativeTheme::GetSystemColor(ColorId color_id,
                                    ColorScheme color_scheme) const {
  // TODO(http://crbug.com/1057754): Remove the below restrictions.
  if (base::FeatureList::IsEnabled(features::kColorProviderRedirection) &&
      !ShouldUseDarkColors() && !UsesHighContrastColors()) {
    if (!color_provider_) {
      // Lazy init the color provider as it makes USER32 calls underneath on
      // Windows, which isn't permitted on renderers.
      // TODO(http://crbug.com/1057754): Handle dark and high contrast modes.
      color_provider_ = ColorProviderManager::Get().GetColorProviderFor(
          ColorProviderManager::ColorMode::kLight,
          ColorProviderManager::ContrastMode::kNormal);
    }
    switch (color_id) {
      case kColorId_WindowBackground:
        return color_provider_->GetColor(kColorWindowBackground);
      case kColorId_DialogBackground:
        return color_provider_->GetColor(kColorDialogBackground);
      default:
        break;
    }
  }
  return GetAuraColor(color_id, this, color_scheme);
}

float NativeTheme::GetBorderRadiusForPart(Part part,
                                          float width,
                                          float height,
                                          float zoom) const {
  return 0;
}

void NativeTheme::AddObserver(NativeThemeObserver* observer) {
  native_theme_observers_.AddObserver(observer);
}

void NativeTheme::RemoveObserver(NativeThemeObserver* observer) {
  native_theme_observers_.RemoveObserver(observer);
}

void NativeTheme::NotifyObservers() {
  for (NativeThemeObserver& observer : native_theme_observers_)
    observer.OnNativeThemeUpdated(this);
}

NativeTheme::NativeTheme(bool should_use_dark_colors)
    : should_use_dark_colors_(should_use_dark_colors || IsForcedDarkMode()),
      is_high_contrast_(IsForcedHighContrast()),
      preferred_color_scheme_(CalculatePreferredColorScheme()) {
#if !defined(OS_ANDROID)
  // TODO(http://crbug.com/1057754): Merge this into the ColorProviderManager.
  static base::OnceClosure color_provider_manager_init = base::BindOnce([]() {
    ColorProviderManager::Get().SetColorProviderInitializer(base::BindRepeating(
        [](ColorProvider* provider, ColorProviderManager::ColorMode color_mode,
           ColorProviderManager::ContrastMode contrast_mode) {
          ui::AddCoreDefaultColorMixers(
              provider, color_mode == ColorProviderManager::ColorMode::kDark);
          ui::AddNativeColorMixers(provider);
          ui::AddUiColorMixers(provider);
        }));
  });
  if (!color_provider_manager_init.is_null())
    std::move(color_provider_manager_init).Run();
#endif  // !defined(OS_ANDROID)
}

NativeTheme::~NativeTheme() = default;

bool NativeTheme::ShouldUseDarkColors() const {
  return should_use_dark_colors_;
}

bool NativeTheme::UsesHighContrastColors() const {
  return is_high_contrast_;
}

NativeTheme::PreferredColorScheme NativeTheme::GetPreferredColorScheme() const {
  return preferred_color_scheme_;
}

bool NativeTheme::IsForcedDarkMode() const {
  static bool kIsForcedDarkMode =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceDarkMode);
  return kIsForcedDarkMode;
}

bool NativeTheme::IsForcedHighContrast() const {
  static bool kIsForcedHighContrast =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceHighContrast);
  return kIsForcedHighContrast;
}

NativeTheme::PreferredColorScheme NativeTheme::CalculatePreferredColorScheme()
    const {
  return ShouldUseDarkColors() ? NativeTheme::PreferredColorScheme::kDark
                               : NativeTheme::PreferredColorScheme::kLight;
}

base::Optional<CaptionStyle> NativeTheme::GetSystemCaptionStyle() const {
  return CaptionStyle::FromSystemSettings();
}

const std::map<NativeTheme::SystemThemeColor, SkColor>&
NativeTheme::GetSystemColors() const {
  return system_colors_;
}

base::Optional<SkColor> NativeTheme::GetSystemThemeColor(
    SystemThemeColor theme_color) const {
  auto color = system_colors_.find(theme_color);
  if (color != system_colors_.end())
    return color->second;

  return base::nullopt;
}

bool NativeTheme::HasDifferentSystemColors(
    const std::map<NativeTheme::SystemThemeColor, SkColor>& colors) const {
  return system_colors_ != colors;
}

void NativeTheme::set_system_colors(
    const std::map<NativeTheme::SystemThemeColor, SkColor>& colors) {
  system_colors_ = colors;
}

bool NativeTheme::UpdateSystemColorInfo(
    bool is_dark_mode,
    bool is_high_contrast,
    const base::flat_map<SystemThemeColor, uint32_t>& colors) {
  bool did_system_color_info_change = false;
  if (is_dark_mode != ShouldUseDarkColors()) {
    did_system_color_info_change = true;
    set_use_dark_colors(is_dark_mode);
  }
  if (is_high_contrast != UsesHighContrastColors()) {
    did_system_color_info_change = true;
    set_high_contrast(is_high_contrast);
  }
  for (const auto& color : colors) {
    if (color.second != GetSystemThemeColor(color.first)) {
      did_system_color_info_change = true;
      system_colors_[color.first] = color.second;
    }
  }
  return did_system_color_info_change;
}

NativeTheme::ColorSchemeNativeThemeObserver::ColorSchemeNativeThemeObserver(
    NativeTheme* theme_to_update)
    : theme_to_update_(theme_to_update) {}

NativeTheme::ColorSchemeNativeThemeObserver::~ColorSchemeNativeThemeObserver() =
    default;

void NativeTheme::ColorSchemeNativeThemeObserver::OnNativeThemeUpdated(
    ui::NativeTheme* observed_theme) {
  bool should_use_dark_colors = observed_theme->ShouldUseDarkColors();
  bool is_high_contrast = observed_theme->UsesHighContrastColors();
  PreferredColorScheme preferred_color_scheme =
      observed_theme->GetPreferredColorScheme();
  bool notify_observers = false;

  if (theme_to_update_->ShouldUseDarkColors() != should_use_dark_colors) {
    theme_to_update_->set_use_dark_colors(should_use_dark_colors);
    notify_observers = true;
  }
  if (theme_to_update_->UsesHighContrastColors() != is_high_contrast) {
    theme_to_update_->set_high_contrast(is_high_contrast);
    notify_observers = true;
  }
  if (theme_to_update_->GetPreferredColorScheme() != preferred_color_scheme) {
    theme_to_update_->set_preferred_color_scheme(preferred_color_scheme);
    notify_observers = true;
  }

  const auto& system_colors = observed_theme->GetSystemColors();
  if (theme_to_update_->HasDifferentSystemColors(system_colors)) {
    theme_to_update_->set_system_colors(system_colors);
    notify_observers = true;
  }

  if (notify_observers)
    theme_to_update_->NotifyObservers();
}

NativeTheme::ColorScheme NativeTheme::GetDefaultSystemColorScheme() const {
  return ShouldUseDarkColors() ? ColorScheme::kDark : ColorScheme::kLight;
}

}  // namespace ui
