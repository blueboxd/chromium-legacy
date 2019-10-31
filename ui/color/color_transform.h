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

// A transform which modifies the result of |foreground_transform| to contrast
// with the result of |background_transform| by at least |contrast_ratio|, if
// possible.  If |high_contrast_foreground_transform| is non-null, its result is
// used as the blend target.
COMPONENT_EXPORT(COLOR)
ColorTransform BlendForMinContrast(
    ColorTransform foreground_transform,
    ColorTransform background_transform,
    base::Optional<ColorTransform> high_contrast_foreground_transform =
        base::nullopt,
    float contrast_ratio = color_utils::kMinimumReadableContrastRatio);

// A transform which blends the result of |transform| toward the color with max
// contrast by |alpha|.
COMPONENT_EXPORT(COLOR)
ColorTransform BlendTowardMaxContrast(ColorTransform transform, SkAlpha alpha);

// A transform which computes the contrast of the result of |transform| against
// the color with max contrast; then returns a color with that same contrast,
// but against the opposite endpoint.
COMPONENT_EXPORT(COLOR) ColorTransform ContrastInvert(ColorTransform transform);

// A transform which returns the default icon color for the result of
// |transform|.
COMPONENT_EXPORT(COLOR)
ColorTransform DeriveDefaultIconColor(ColorTransform transform);

// A transform which outputs |color|.
COMPONENT_EXPORT(COLOR) ColorTransform FromColor(SkColor color);

// A transform which returns the color |id| from set |set_id|.
COMPONENT_EXPORT(COLOR)
ColorTransform FromOriginalColorFromSet(ColorId id, ColorSetId set_id);

// A transform which returns the input color |id|.
COMPONENT_EXPORT(COLOR) ColorTransform FromInputColor(ColorId id);

// A transform which returns the result color |id|.
COMPONENT_EXPORT(COLOR) ColorTransform FromResultColor(ColorId id);

// A transform which returns the transform's input color (i.e. does nothing).
// This is useful to supply as an argument to other transforms to control how
// the input color is routed.
COMPONENT_EXPORT(COLOR) ColorTransform FromTransformInput();

// A transform which returns the color with max contrast against the result of
// |transform|.
COMPONENT_EXPORT(COLOR)
ColorTransform GetColorWithMaxContrast(ColorTransform transform);

// A transform which returns the resulting paint color of the result of
// |foreground_transform| over the result of |background_transform|.
COMPONENT_EXPORT(COLOR)
ColorTransform GetResultingPaintColor(ColorTransform foreground_transform,
                                      ColorTransform background_transform);

// A transform which runs one of two output transforms based on whether the
// result of |input_transform| is dark.
COMPONENT_EXPORT(COLOR)
ColorTransform SelectBasedOnDarkInput(
    ColorTransform input_transform,
    ColorTransform output_transform_for_dark_input,
    ColorTransform output_transform_for_light_input);

}  // namespace ui

#endif  // UI_COLOR_COLOR_TRANSFORM_H_
