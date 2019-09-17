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

const int kAlphaThreshold = 100;
const int kBrightnessThreshold = 50;

// TODO(https://crbug.com/925949): Add detection and classification of
// background image color. Most sites with dark background images also have a
// dark background color set, so this is less of a priority than it would be
// otherwise.
bool HasLightBackground(const LayoutView& root) {
  const ComputedStyle& style = root.StyleRef();

  // If we can't easily determine the background color, default to inverting the
  // page.
  if (!style.HasBackground())
    return true;

  Color color = style.VisitedDependentColor(GetCSSPropertyBackgroundColor());
  if (color.Alpha() < kAlphaThreshold)
    return true;

  return DarkModeColorClassifier::CalculateColorBrightness(color) >
         kBrightnessThreshold;
}

bool IsDarkModeEnabled(const Settings& frame_settings) {
  static bool isDarkModeEnabledByFeatureFlag =
      features::kForceDarkInversionMethodParam.Get() !=
      ForceDarkInversionMethod::kUseBlinkSettings;
  return isDarkModeEnabledByFeatureFlag || frame_settings.GetDarkModeEnabled();
}

DarkModeInversionAlgorithm GetMode(const Settings& frame_settings) {
  switch (features::kForceDarkInversionMethodParam.Get()) {
    case ForceDarkInversionMethod::kUseBlinkSettings:
      return frame_settings.GetDarkModeInversionAlgorithm();
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
      return frame_settings.GetDarkModeImagePolicy();
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
  return flag_value >= 0 ? flag_value
                         : frame_settings.GetDarkModeTextBrightnessThreshold();
}

int GetBackgroundBrightnessThreshold(const Settings& frame_settings) {
  const int flag_value = base::GetFieldTrialParamByFeatureAsInt(
      features::kForceWebContentsDarkMode,
      features::kForceDarkBackgroundLightnessThresholdParam.name, -1);
  return flag_value >= 0
             ? flag_value
             : frame_settings.GetDarkModeBackgroundBrightnessThreshold();
}

// A static wrapper to hold DarkModeSettings after initial creation.
//
// These settings are fixed after the browser launches, so they only need to be
// constructed once for the "enabled" state and once for the "disabled" state.
// Caching these values instead of re-calculating them should improve page load
// performance.
const DarkModeSettings& GetCachedEnabledSettings(
    const Settings& frame_settings) {
  static const DarkModeSettings* settings =
      new DarkModeSettings([](const Settings& frame_settings) {
        DarkModeSettings settings;
        settings.mode = GetMode(frame_settings);
        settings.image_policy = GetImagePolicy(frame_settings);
        settings.text_brightness_threshold =
            GetTextBrightnessThreshold(frame_settings);
        settings.background_brightness_threshold =
            GetBackgroundBrightnessThreshold(frame_settings);

        settings.grayscale = frame_settings.GetDarkModeGrayscale();
        settings.contrast = frame_settings.GetDarkModeContrast();
        settings.image_grayscale_percent =
            frame_settings.GetDarkModeImageGrayscale();
        return settings;
      }(frame_settings));
  return *settings;
}

// In theory it should be sufficient for the disabled settings to set mode to
// kOff (or to just return the default struct) without also setting
// image_policy. However, this causes images to be inverted unexpectedly in
// some cases (such as when toggling between the site's light and dark theme
// on arstechnica.com).
//
// TODO(gilmanmh): Investigate unexpected image inversion behavior when
// image_policy not set to kFilterNone.
const DarkModeSettings& GetCachedDisabledSettings() {
  static const DarkModeSettings* settings = new DarkModeSettings([]() {
    DarkModeSettings settings;
    settings.mode = DarkModeInversionAlgorithm::kOff;
    settings.image_policy = DarkModeImagePolicy::kFilterNone;
    return settings;
  }());
  return *settings;
}

}  // namespace

DarkModeSettings BuildDarkModeSettings(const Settings& frame_settings,
                                       const LayoutView& root) {
  if (IsDarkModeEnabled(frame_settings) &&
      ShouldApplyDarkModeFilterToPage(frame_settings.GetDarkModePagePolicy(),
                                      root)) {
    return GetCachedEnabledSettings(frame_settings);
  }
  return GetCachedDisabledSettings();
}

bool ShouldApplyDarkModeFilterToPage(DarkModePagePolicy policy,
                                     const LayoutView& root) {
  if (root.StyleRef().DarkColorScheme())
    return false;

  switch (policy) {
    case DarkModePagePolicy::kFilterAll:
      return true;
    case DarkModePagePolicy::kFilterByBackground:
      return HasLightBackground(root);
  }
}

}  // namespace blink
