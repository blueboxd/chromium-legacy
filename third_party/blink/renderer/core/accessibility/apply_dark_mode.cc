// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/accessibility/apply_dark_mode.h"

#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_color_classifier.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_settings.h"

namespace blink {
namespace {

// TODO(https://crbug.com/925949): Add detection and classification of
// background image color. Most sites with dark background images also have a
// dark background color set, so this is less of a priority than it would be
// otherwise.
bool HasLightBackground(const LayoutView& root) {
  const ComputedStyle& style = root.StyleRef();
  if (style.HasBackground()) {
    Color color = style.VisitedDependentColor(GetCSSPropertyBackgroundColor());
    return IsLight(color);
  }

  // If we can't easily determine the background color, default to inverting the
  // page.
  return true;
}

}  // namespace

DarkModeSettings BuildDarkModeSettings(const Settings& frame_settings,
                                       const LayoutView& root) {
  DarkModeSettings dark_mode_settings;

  if (!ShouldApplyDarkModeFilterToPage(frame_settings.GetDarkModePagePolicy(),
                                       root)) {
    // In theory it should be sufficient to set mode to
    // kOff (or to just return the default struct) without also setting
    // image_policy. However, this causes images to be inverted unexpectedly in
    // some cases (such as when toggling between the site's light and dark theme
    // on arstechnica.com).
    //
    // TODO(gilmanmh): Investigate unexpected image inversion behavior when
    // image_policy not set to kFilterNone.
    dark_mode_settings.mode = DarkMode::kOff;
    dark_mode_settings.image_policy = DarkModeImagePolicy::kFilterNone;
    return dark_mode_settings;
  }
  dark_mode_settings.mode = frame_settings.GetDarkMode();
  dark_mode_settings.grayscale = frame_settings.GetDarkModeGrayscale();
  dark_mode_settings.contrast = frame_settings.GetDarkModeContrast();
  dark_mode_settings.image_policy = frame_settings.GetDarkModeImagePolicy();
  dark_mode_settings.text_brightness_threshold =
      frame_settings.GetDarkModeTextBrightnessThreshold();
  dark_mode_settings.background_brightness_threshold =
      frame_settings.GetDarkModeBackgroundBrightnessThreshold();
  dark_mode_settings.image_grayscale_percent =
      frame_settings.GetDarkModeImageGrayscale();
  return dark_mode_settings;
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
