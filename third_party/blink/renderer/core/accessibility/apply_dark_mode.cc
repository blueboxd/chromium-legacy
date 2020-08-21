// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/accessibility/apply_dark_mode.h"

#include "base/metrics/field_trial_params.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/forcedark/forcedark_switches.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_color_classifier.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_settings.h"

namespace blink {
namespace {

DarkModeInversionAlgorithm GetMode(const Settings& frame_settings) {
  switch (features::kForceDarkInversionMethodParam.Get()) {
    case ForceDarkInversionMethod::kUseBlinkSettings:
      return frame_settings.GetForceDarkModeInversionAlgorithm();
    case ForceDarkInversionMethod::kCielabBased:
      return DarkModeInversionAlgorithm::kInvertLightnessLAB;
    case ForceDarkInversionMethod::kHslBased:
      return DarkModeInversionAlgorithm::kInvertLightness;
    case ForceDarkInversionMethod::kRgbBased:
      return DarkModeInversionAlgorithm::kInvertBrightness;
  }
}

DarkModeImagePolicy GetImagePolicy(const Settings& frame_settings) {
  switch (features::kForceDarkImageBehaviorParam.Get()) {
    case ForceDarkImageBehavior::kUseBlinkSettings:
      return frame_settings.GetForceDarkModeImagePolicy();
    case ForceDarkImageBehavior::kInvertNone:
      return DarkModeImagePolicy::kFilterNone;
    case ForceDarkImageBehavior::kInvertSelectively:
      return DarkModeImagePolicy::kFilterSmart;
  }
}

int GetTextBrightnessThreshold(const Settings& frame_settings) {
  const int flag_value = base::GetFieldTrialParamByFeatureAsInt(
      features::kForceWebContentsDarkMode,
      features::kForceDarkTextLightnessThresholdParam.name, -1);
  return flag_value >= 0
             ? flag_value
             : frame_settings.GetForceDarkModeTextBrightnessThreshold();
}

int GetBackgroundBrightnessThreshold(const Settings& frame_settings) {
  const int flag_value = base::GetFieldTrialParamByFeatureAsInt(
      features::kForceWebContentsDarkMode,
      features::kForceDarkBackgroundLightnessThresholdParam.name, -1);
  return flag_value >= 0
             ? flag_value
             : frame_settings.GetForceDarkModeBackgroundBrightnessThreshold();
}

DarkModeSettings GetEnabledSettings(const Settings& frame_settings) {
  DarkModeSettings settings;
  settings.mode = GetMode(frame_settings);
  settings.image_policy = GetImagePolicy(frame_settings);
  settings.text_brightness_threshold =
      GetTextBrightnessThreshold(frame_settings);
  settings.background_brightness_threshold =
      GetBackgroundBrightnessThreshold(frame_settings);

  settings.grayscale = frame_settings.GetForceDarkModeGrayscale();
  settings.contrast = frame_settings.GetForceDarkModeContrast();
  settings.image_grayscale_percent =
      frame_settings.GetForceDarkModeImageGrayscale();
  return settings;
}

DarkModeSettings GetDisabledSettings() {
  DarkModeSettings settings;
  settings.mode = DarkModeInversionAlgorithm::kOff;
  return settings;
}

}  // namespace

DarkModeSettings BuildDarkModeSettings(const Settings& frame_settings,
                                       bool content_has_dark_color_scheme) {
  if (content_has_dark_color_scheme)
    return GetDisabledSettings();

  return frame_settings.GetForceDarkModeEnabled()
             ? GetEnabledSettings(frame_settings)
             : GetDisabledSettings();
}

}  // namespace blink
