// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CROSAPI_MOJOM_BITMAP_MOJOM_TRAITS_H_
#define CHROMEOS_CROSAPI_MOJOM_BITMAP_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "chromeos/crosapi/cpp/bitmap.h"
#include "chromeos/crosapi/mojom/bitmap.mojom-shared.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(CROSAPI_MOJOM_TRAITS)
    StructTraits<crosapi::mojom::BitmapDataView, crosapi::Bitmap> {
  static uint32_t width(const crosapi::Bitmap& bitmap) { return bitmap.width; }
  static uint32_t height(const crosapi::Bitmap& bitmap) {
    return bitmap.height;
  }
  static const std::vector<uint8_t>& pixels(const crosapi::Bitmap& bitmap) {
    return bitmap.pixels;
  }
  static bool Read(crosapi::mojom::BitmapDataView, crosapi::Bitmap* out);
};

}  // namespace mojo

#endif  // CHROMEOS_CROSAPI_MOJOM_BITMAP_MOJOM_TRAITS_H_
