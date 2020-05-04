// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/caption_util.h"

#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/common/pref_names_util.h"
#include "components/prefs/pref_service.h"
#include "ui/base/ui_base_switches.h"
#include "ui/native_theme/native_theme.h"

namespace captions {

base::Optional<ui::CaptionStyle> GetCaptionStyleFromUserSettings(
    PrefService* prefs,
    bool record_metrics) {
  // Apply native CaptionStyle parameters.
  base::Optional<ui::CaptionStyle> style;

  // Apply native CaptionStyle parameters.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kForceCaptionStyle)) {
    style = ui::CaptionStyle::FromSpec(
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kForceCaptionStyle));
  }

  // Apply system caption style.
  if (!style) {
    ui::NativeTheme* native_theme = ui::NativeTheme::GetInstanceForWeb();
    style = native_theme->GetSystemCaptionStyle();
    if (record_metrics) {
      base::UmaHistogramBoolean(
          "Accessibility.CaptionSettingsLoadedFromSystemSettings",
          style.has_value());
    }
  }

  // Apply caption style from preferences if system caption style is undefined.
  if (!style) {
    style = pref_names_util::GetCaptionStyleFromPrefs(prefs);
    if (record_metrics) {
      base::UmaHistogramBoolean("Accessibility.CaptionSettingsLoadedFromPrefs",
                                style.has_value());
    }
  }

  return style;
}

}  // namespace captions
