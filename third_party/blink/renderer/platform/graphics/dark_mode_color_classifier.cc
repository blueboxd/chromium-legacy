// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/dark_mode_color_classifier.h"

namespace blink {
namespace {

class SimpleColorClassifier : public DarkModeColorClassifier {
 public:
  static std::unique_ptr<SimpleColorClassifier> NeverInvert() {
    return std::unique_ptr<SimpleColorClassifier>(
        new SimpleColorClassifier(false));
  }

  static std::unique_ptr<SimpleColorClassifier> AlwaysInvert() {
    return std::unique_ptr<SimpleColorClassifier>(
        new SimpleColorClassifier(true));
  }

  bool ShouldInvertColor(const Color& color) override { return value_; }

 private:
  SimpleColorClassifier(bool value) : value_(value) {}

  bool value_;
};

class InvertLowBrightnessColorsClassifier : public DarkModeColorClassifier {
 public:
  bool ShouldInvertColor(const Color& color) override {
    if (color == Color::kWhite) {
      return false;
    }
    return true;
  }
};

}  // namespace

// Values below which a color is considered sufficiently transparent that a
// lighter color behind it would make the final color as seen by the user light.
// TODO(https://crbug.com/925949): This is a placeholder value. It should be
// replaced with a better researched value before launching dark mode.
const float kOpacityThreshold = 0.4;

// TODO(https://crbug.com/925949): Find a better algorithm for this.
bool IsLight(const Color& color) {
  // Use the alpha value as the opacity.
  float opacity = (color.Alpha() / 255);
  // Assume the color is light if the background is sufficiently transparent.
  if (opacity < kOpacityThreshold) {
    return true;
  }
  double hue;
  double saturation;
  double lightness;
  color.GetHSL(hue, saturation, lightness);
  return lightness > 0.5;
}

std::unique_ptr<DarkModeColorClassifier>
DarkModeColorClassifier::MakeTextColorClassifier(
    const DarkModeSettings& settings) {
  if (settings.text_policy == DarkModeTextPolicy::kInvertAll)
    return SimpleColorClassifier::AlwaysInvert();

  // Throw an error in debug mode if new values are added to the enum without
  // updating this method.
  DCHECK_EQ(settings.text_policy, DarkModeTextPolicy::kInvertDarkOnly);
  return std::make_unique<InvertLowBrightnessColorsClassifier>();
}

DarkModeColorClassifier::~DarkModeColorClassifier() {}

}  // namespace blink
