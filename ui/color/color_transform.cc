// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_transform.h"

#include "base/bind.h"
#include "ui/color/color_mixer.h"

namespace ui {

ColorTransform BlendForMinContrast(
    ColorId background_id,
    base::Optional<ColorId> high_contrast_foreground_id,
    float contrast_ratio) {
  DCHECK_COLOR_ID_VALID(background_id);
  const auto generator = [](ColorId background_id,
                            base::Optional<ColorId> high_contrast_foreground_id,
                            float contrast_ratio, SkColor input_color,
                            const ColorMixer& mixer) {
    const SkColor background_color = mixer.GetResultColor(background_id);
    const base::Optional<SkColor> foreground_color =
        high_contrast_foreground_id.has_value()
            ? base::make_optional(
                  mixer.GetResultColor(high_contrast_foreground_id.value()))
            : base::nullopt;
    return color_utils::BlendForMinContrast(input_color, background_color,
                                            foreground_color, contrast_ratio)
        .color;
  };
  return base::Bind(generator, background_id, high_contrast_foreground_id,
                    contrast_ratio);
}

ColorTransform BlendTowardMaxContrast(SkAlpha alpha) {
  const auto generator = [](SkAlpha alpha, SkColor input_color,
                            const ColorMixer& mixer) {
    return color_utils::BlendTowardMaxContrast(input_color, alpha);
  };
  return base::Bind(generator, alpha);
}

ColorTransform DeriveDefaultIconColor() {
  const auto generator = [](SkColor input_color, const ColorMixer& mixer) {
    return color_utils::DeriveDefaultIconColor(input_color);
  };
  return base::Bind(generator);
}

ColorTransform FromColor(SkColor color) {
  const auto generator = [](SkColor color, SkColor input_color,
                            const ColorMixer& mixer) { return color; };
  return base::Bind(generator, color);
}

ColorTransform FromOriginalColorFromSet(ColorId id, ColorSetId set_id) {
  DCHECK_COLOR_ID_VALID(id);
  DCHECK_COLOR_SET_ID_VALID(set_id);
  const auto generator = [](ColorId id, ColorSetId set_id, SkColor input_color,
                            const ColorMixer& mixer) {
    return mixer.GetOriginalColorFromSet(id, set_id);
  };
  return base::Bind(generator, id, set_id);
}

ColorTransform FromInputColor(ColorId id) {
  DCHECK_COLOR_ID_VALID(id);
  const auto generator = [](ColorId id, SkColor input_color,
                            const ColorMixer& mixer) {
    return mixer.GetInputColor(id);
  };
  return base::Bind(generator, id);
}

ColorTransform GetColorWithMaxContrast() {
  const auto generator = [](SkColor input_color, const ColorMixer& mixer) {
    return color_utils::GetColorWithMaxContrast(input_color);
  };
  return base::Bind(generator);
}

}  // namespace ui
