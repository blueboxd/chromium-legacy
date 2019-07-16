// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme.h"

#include <cstring>

#include "base/command_line.h"
#include "ui/base/ui_base_switches.h"

namespace ui {

NativeTheme::ExtraParams::ExtraParams() {
  memset(this, 0, sizeof(*this));
}

NativeTheme::ExtraParams::ExtraParams(const ExtraParams& other) {
  memcpy(this, &other, sizeof(*this));
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

NativeTheme::NativeTheme()
    : is_dark_mode_(IsForcedDarkMode()),
      is_high_contrast_(IsForcedHighContrast()),
      preferred_color_scheme_(CalculatePreferredColorScheme()) {}

NativeTheme::~NativeTheme() = default;

bool NativeTheme::SystemDarkModeEnabled() const {
  return is_dark_mode_;
}

bool NativeTheme::SystemDarkModeSupported() const {
  return false;
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
  return SystemDarkModeEnabled() ? NativeTheme::PreferredColorScheme::kDark
                                 : NativeTheme::PreferredColorScheme::kLight;
}

base::Optional<CaptionStyle> NativeTheme::GetSystemCaptionStyle() const {
  return CaptionStyle::FromSystemSettings();
}

NativeTheme::ColorSchemeNativeThemeObserver::ColorSchemeNativeThemeObserver(
    NativeTheme* theme_to_update)
    : theme_to_update_(theme_to_update) {}

NativeTheme::ColorSchemeNativeThemeObserver::~ColorSchemeNativeThemeObserver() =
    default;

void NativeTheme::ColorSchemeNativeThemeObserver::OnNativeThemeUpdated(
    ui::NativeTheme* observed_theme) {
  bool is_dark_mode = observed_theme->SystemDarkModeEnabled();
  bool is_high_contrast = observed_theme->UsesHighContrastColors();
  PreferredColorScheme preferred_color_scheme =
      observed_theme->GetPreferredColorScheme();
  bool notify_observers = false;

  if (theme_to_update_->SystemDarkModeEnabled() != is_dark_mode) {
    theme_to_update_->set_dark_mode(is_dark_mode);
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

  if (notify_observers)
    theme_to_update_->NotifyObservers();
}

}  // namespace ui
