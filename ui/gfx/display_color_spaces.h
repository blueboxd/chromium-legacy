// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_DISPLAY_COLOR_SPACES_H_
#define UI_GFX_DISPLAY_COLOR_SPACES_H_

#include "ui/gfx/color_space.h"
#include "ui/gfx/color_space_export.h"

namespace gfx {

// This structure is used by a display::Display to specify the color space that
// should be used to display content of various types. This lives in here, as
// opposed to in ui/display because it is used directly by components/viz.
struct COLOR_SPACE_EXPORT DisplayColorSpaces {
  // Initialize as sRGB-only.
  DisplayColorSpaces();

  // Initialize as |color_space| for all settings.
  explicit DisplayColorSpaces(const ColorSpace& color_space);

  // Return the color space that should be used for rasterization.
  gfx::ColorSpace GetRasterColorSpace() const;

  // Return the color space in which compositing (and, in particular, blending,
  // should be performed). This space may not (on Windows) be suitable for
  // output.
  // TODO: This will take arguments regarding the presence of WCG and HDR
  // content, and whether or not an alpha channel will be needed.
  gfx::ColorSpace GetCompositingColorSpace() const;

  // Return the color space to use for output.
  gfx::ColorSpace GetOutputColorSpace(bool needs_alpha) const;

  // Return true if |color_space| is a valid output color space. If it is not,
  // and is the color space for the root RenderPass, then an additional pass
  // will be added to a color space from GetOutputColorSpace.
  bool IsSuitableForOutput(const gfx::ColorSpace& color_space) const;

  // Return true if the HDR color spaces are, indeed, HDR.
  bool SupportsHDR() const;

  bool operator==(const DisplayColorSpaces& other) const;
  bool operator!=(const DisplayColorSpaces& other) const;

  // The color space to use for SDR content that is limited to the sRGB gamut.
  ColorSpace srgb = ColorSpace::CreateSRGB();

  // For opaque and transparent SDR content that is larger than the sRGB gamut.
  ColorSpace wcg_opaque = ColorSpace::CreateSRGB();
  ColorSpace wcg_transparent = ColorSpace::CreateSRGB();

  // For opaque and transparent HDR content.
  ColorSpace hdr_opaque = ColorSpace::CreateSRGB();
  ColorSpace hdr_transparent = ColorSpace::CreateSRGB();

  // The SDR white level in nits. This varies only on Windows.
  float sdr_white_level = ColorSpace::kDefaultSDRWhiteLevel;
};

}  // namespace gfx

#endif  // UI_GFX_DISPLAY_COLOR_SPACES_H_
