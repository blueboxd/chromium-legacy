// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mojom/display_color_spaces_mojom_traits.h"

namespace mojo {

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
