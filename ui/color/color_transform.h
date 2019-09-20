// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_COLOR_TRANSFORM_H_
#define UI_COLOR_COLOR_TRANSFORM_H_

#include "base/callback.h"
#include "base/component_export.h"
#include "base/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/gfx/color_utils.h"

namespace ui {

class ColorMixer;

// ColorTransform is a function which transforms an |input| color, optionally
// using a |mixer| (to obtain other colors).  ColorTransforms can be chained
// together in ColorRecipes, where each will be applied to the preceding
// transform's output.
using ColorTransform =
    base::RepeatingCallback<SkColor(SkColor input, const ColorMixer& mixer)>;

// Functions to create common transform:

// A transform which modifies the input color to contrast with result color
// |background_id| by at least |contrast_ratio|, if possible.  If
// |high_contrast_foreground_id| is non-null, it is used as the blend target.
COMPONENT_EXPORT(COLOR)
ColorTransform BlendForMinContrast(
    ColorId background_id,
    base::Optional<ColorId> high_contrast_foreground_id = base::nullopt,
    float contrast_ratio = color_utils::kMinimumReadableContrastRatio);

// A transform which blends the input color toward the color with max contrast
// by |alpha|.
COMPONENT_EXPORT(COLOR) ColorTransform BlendTowardMaxContrast(SkAlpha alpha);

// A transform which returns the default icon color for the input color.
COMPONENT_EXPORT(COLOR) ColorTransform DeriveDefaultIconColor();

// A transform which outputs |color|.
COMPONENT_EXPORT(COLOR) ColorTransform FromColor(SkColor color);

// A transform which returns the color |id| from set |set_id|.
COMPONENT_EXPORT(COLOR)
ColorTransform FromOriginalColorFromSet(ColorId id, ColorSetId set_id);

// A transform which returns the input color |id|.
COMPONENT_EXPORT(COLOR) ColorTransform FromInputColor(ColorId id);

// A transform which returns the color with max contrast against the input
// color.
COMPONENT_EXPORT(COLOR) ColorTransform GetColorWithMaxContrast();

}  // namespace ui

#endif  // UI_COLOR_COLOR_TRANSFORM_H_
