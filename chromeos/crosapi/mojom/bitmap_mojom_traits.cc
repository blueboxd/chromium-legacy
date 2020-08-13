// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/crosapi/mojom/bitmap_mojom_traits.h"

#include "base/numerics/checked_math.h"

namespace mojo {

// static
bool StructTraits<crosapi::mojom::BitmapDataView, crosapi::Bitmap>::Read(
    crosapi::mojom::BitmapDataView data,
    crosapi::Bitmap* out) {
  out->width = data.width();
  out->height = data.height();

  ArrayDataView<uint8_t> pixels;
  data.GetPixelsDataView(&pixels);

  uint32_t size;
  size = base::CheckMul(out->width, out->height).ValueOrDie();
  size = base::CheckMul(size, 4).ValueOrDie();

  if (pixels.size() != base::checked_cast<size_t>(size))
    return false;

  const uint8_t* base = pixels.data();
  out->pixels.assign(base, base + pixels.size());
  return true;
}

}  // namespace mojo
