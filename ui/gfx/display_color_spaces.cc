// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/display_color_spaces.h"

namespace gfx {

DisplayColorSpaces::DisplayColorSpaces() = default;

DisplayColorSpaces::DisplayColorSpaces(const gfx::ColorSpace& c)
    : srgb(c),
      wcg_opaque(c),
      wcg_transparent(c),
      hdr_opaque(c),
      hdr_transparent(c) {}

gfx::ColorSpace DisplayColorSpaces::GetRasterColorSpace() const {
  return hdr_transparent.GetRasterColorSpace();
}

gfx::ColorSpace DisplayColorSpaces::GetCompositingColorSpace() const {
  if (SupportsHDR())
    return gfx::ColorSpace::CreateExtendedSRGB();
  return hdr_transparent;
}

gfx::ColorSpace DisplayColorSpaces::GetOutputColorSpace(
    bool needs_alpha) const {
  if (needs_alpha)
    return hdr_transparent;
  else
    return hdr_opaque;
}

bool DisplayColorSpaces::IsSuitableForOutput(
    const gfx::ColorSpace& color_space) const {
  return color_space == hdr_opaque || color_space == hdr_transparent;
}

bool DisplayColorSpaces::SupportsHDR() const {
  return hdr_opaque.IsHDR() && hdr_transparent.IsHDR();
}

bool DisplayColorSpaces::operator==(const DisplayColorSpaces& other) const {
  return srgb == other.srgb && wcg_opaque == other.wcg_opaque &&
         wcg_transparent == other.wcg_transparent &&
         hdr_opaque == other.hdr_opaque &&
         hdr_transparent == other.hdr_transparent &&
         sdr_white_level == other.sdr_white_level;
}

bool DisplayColorSpaces::operator!=(const DisplayColorSpaces& other) const {
  return !(*this == other);
}

}  // namespace gfx
