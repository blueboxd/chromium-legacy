// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mojom/display_color_spaces_mojom_traits.h"

namespace mojo {

// static
gfx::mojom::ContentColorUsage
EnumTraits<gfx::mojom::ContentColorUsage, gfx::ContentColorUsage>::ToMojom(
    gfx::ContentColorUsage input) {
  switch (input) {
    case gfx::ContentColorUsage::kSRGB:
      return gfx::mojom::ContentColorUsage::kSRGB;
    case gfx::ContentColorUsage::kWideColorGamut:
      return gfx::mojom::ContentColorUsage::kWideColorGamut;
    case gfx::ContentColorUsage::kHDR:
      return gfx::mojom::ContentColorUsage::kHDR;
  }
  NOTREACHED();
  return gfx::mojom::ContentColorUsage::kSRGB;
}

// static
bool EnumTraits<gfx::mojom::ContentColorUsage, gfx::ContentColorUsage>::
    FromMojom(gfx::mojom::ContentColorUsage input,
              gfx::ContentColorUsage* output) {
  switch (input) {
    case gfx::mojom::ContentColorUsage::kSRGB:
      *output = gfx::ContentColorUsage::kSRGB;
      return true;
    case gfx::mojom::ContentColorUsage::kWideColorGamut:
      *output = gfx::ContentColorUsage::kWideColorGamut;
      return true;
    case gfx::mojom::ContentColorUsage::kHDR:
      *output = gfx::ContentColorUsage::kHDR;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
bool StructTraits<
    gfx::mojom::DisplayColorSpacesDataView,
    gfx::DisplayColorSpaces>::Read(gfx::mojom::DisplayColorSpacesDataView input,
                                   gfx::DisplayColorSpaces* out) {
  if (!input.ReadSrgb(&out->srgb))
    return false;
  if (!input.ReadWcgOpaque(&out->wcg_opaque))
    return false;
  if (!input.ReadWcgTransparent(&out->wcg_transparent))
    return false;
  if (!input.ReadHdrOpaque(&out->hdr_opaque))
    return false;
  if (!input.ReadHdrTransparent(&out->hdr_transparent))
    return false;
  out->sdr_white_level = input.sdr_white_level();
  return true;
}

}  // namespace mojo
