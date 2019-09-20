// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_COLOR_RECIPE_H_
#define UI_COLOR_COLOR_RECIPE_H_

#include <list>

#include "base/component_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_transform.h"

namespace ui {

class ColorMixer;

// A ColorRecipe describes how to construct an output color from a series of
// transforms.  Recipes take an input color and mixer, then apply their
// transforms in order to produce the output.  This means that a recipe with no
// transforms will return its input color unchanged.
class COMPONENT_EXPORT(COLOR) ColorRecipe {
 public:
  ColorRecipe();
  // ColorRecipe is movable since it holds the full list of transforms, which
  // might be expensive to copy.
  ColorRecipe(ColorRecipe&&) noexcept;
  ColorRecipe& operator=(ColorRecipe&&) noexcept;
  ~ColorRecipe();

  // Adds a transform to the end of the current recipe.  Returns a non-const ref
  // to allow chaining calls.
  ColorRecipe& AddTransform(ColorTransform&& transform);

  // Generates the output color for |input| by applying all transforms.  |mixer|
  // is passed to each transform, since it might need to request other colors.
  SkColor GenerateResult(SkColor input, const ColorMixer& mixer) const;

 private:
  std::list<ColorTransform> transforms_;
};

}  // namespace ui

#endif  // UI_COLOR_COLOR_RECIPE_H_
