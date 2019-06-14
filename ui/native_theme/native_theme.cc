// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme.h"

#include <cstring>

#include "base/bind.h"
#include "base/command_line.h"
#include "ui/base/ui_base_switches.h"
#include "ui/native_theme/dark_mode_observer.h"
#include "ui/native_theme/native_theme_observer.h"

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
      is_high_contrast_(IsForcedHighContrast()) {}

NativeTheme::~NativeTheme() {
  if (dark_mode_parent_observer_)
    dark_mode_parent_observer_->Stop();
}

bool NativeTheme::SystemDarkModeEnabled() const {
  return is_dark_mode_;
}

bool NativeTheme::SystemDarkModeSupported() const {
  return false;
}

bool NativeTheme::UsesHighContrastColors() const {
  return is_high_contrast_;
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

CaptionStyle NativeTheme::GetSystemCaptionStyle() const {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kForceCaptionStyle)) {
    return CaptionStyle::FromSpec(
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kForceCaptionStyle));
  }

  return CaptionStyle::FromSystemSettings();
}

void NativeTheme::SetDarkModeParent(NativeTheme* dark_mode_parent) {
  dark_mode_parent_observer_ = std::make_unique<DarkModeObserver>(
      dark_mode_parent,
      base::BindRepeating(&NativeTheme::OnParentDarkModeChanged,
                          base::Unretained(this)));
  dark_mode_parent_observer_->Start();
}

void NativeTheme::OnParentDarkModeChanged(bool is_dark_mode) {
  set_dark_mode(is_dark_mode);
  NotifyObservers();
}
}  // namespace ui
