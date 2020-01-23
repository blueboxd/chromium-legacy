// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/display_color_spaces.h"

namespace gfx {

DisplayColorSpaces::DisplayColorSpaces() = default;

DisplayColorSpaces::DisplayColorSpaces(const gfx::ColorSpace& c)
    : srgb(c.IsValid() ? c : gfx::ColorSpace::CreateSRGB()),
      wcg_opaque(srgb),
      wcg_transparent(srgb),
      hdr_opaque(srgb),
      hdr_transparent(srgb) {}

gfx::ColorSpace DisplayColorSpaces::GetRasterColorSpace() const {
  return hdr_opaque.GetRasterColorSpace();
}

gfx::ColorSpace DisplayColorSpaces::GetCompositingColorSpace(
    bool needs_alpha) const {
  const gfx::ColorSpace result = needs_alpha ? hdr_transparent : hdr_opaque;
  if (result.IsHDR()) {
    // PQ is not an acceptable space to do blending in -- blending 0 and 1
    // evenly will get a result of sRGB 0.259 (instead of 0.5).
    if (result.GetTransferID() == gfx::ColorSpace::TransferID::SMPTEST2084)
      return gfx::ColorSpace::CreateExtendedSRGB();

    // If the color space is nearly-linear, then it is not suitable for
    // blending -- blending 0 and 1 evenly will get a result of sRGB 0.735
    // (instead of 0.5).
    skcms_TransferFunction fn;
    if (result.GetTransferFunction(&fn)) {
      constexpr float kMinGamma = 1.25;
      if (fn.g < kMinGamma)
        return gfx::ColorSpace::CreateExtendedSRGB();
    }
  }
  return result;
}

gfx::ColorSpace DisplayColorSpaces::GetOutputColorSpace(
    bool needs_alpha) const {
  if (needs_alpha)
    return hdr_transparent;
  else
    return hdr_opaque;
}

bool DisplayColorSpaces::NeedsHDRColorConversionPass(
    const gfx::ColorSpace& color_space) const {
  return color_space.IsHDR() && color_space != hdr_opaque &&
         color_space != hdr_transparent;
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
