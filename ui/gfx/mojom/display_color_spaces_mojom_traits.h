// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJOM_DISPLAY_COLOR_SPACES_MOJOM_TRAITS_H_
#define UI_GFX_MOJOM_DISPLAY_COLOR_SPACES_MOJOM_TRAITS_H_

#include "base/containers/span.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/gfx/mojom/color_space_mojom_traits.h"
#include "ui/gfx/mojom/display_color_spaces.mojom.h"

namespace mojo {

template <>
struct StructTraits<gfx::mojom::DisplayColorSpacesDataView,
                    gfx::DisplayColorSpaces> {
  static gfx::ColorSpace srgb(const gfx::DisplayColorSpaces& input) {
    return input.srgb;
  }
  static gfx::ColorSpace wcg_opaque(const gfx::DisplayColorSpaces& input) {
    return input.wcg_opaque;
  }
  static gfx::ColorSpace wcg_transparent(const gfx::DisplayColorSpaces& input) {
    return input.wcg_transparent;
  }
  static gfx::ColorSpace hdr_opaque(const gfx::DisplayColorSpaces& input) {
    return input.hdr_opaque;
  }
  static gfx::ColorSpace hdr_transparent(const gfx::DisplayColorSpaces& input) {
    return input.hdr_transparent;
  }
  static float sdr_white_level(const gfx::DisplayColorSpaces& input) {
    return input.sdr_white_level;
  }

  static bool Read(gfx::mojom::DisplayColorSpacesDataView data,
                   gfx::DisplayColorSpaces* out);
};

}  // namespace mojo

#endif  // UI_GFX_MOJOM_DISPLAY_COLOR_SPACES_MOJOM_TRAITS_H_
