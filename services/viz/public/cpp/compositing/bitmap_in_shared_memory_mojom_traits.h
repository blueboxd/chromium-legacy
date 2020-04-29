// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_BITMAP_IN_SHARED_MEMORY_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_BITMAP_IN_SHARED_MEMORY_MOJOM_TRAITS_H_

#include "base/memory/writable_shared_memory_region.h"
#include "mojo/public/cpp/base/shared_memory_mojom_traits.h"
#include "services/viz/public/mojom/compositing/bitmap_in_shared_memory.mojom-shared.h"
#include "skia/public/mojom/image_info_mojom_traits.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"

namespace mojo {

template <>
struct StructTraits<viz::mojom::BitmapInSharedMemoryDataView, SkBitmap> {
  static const SkImageInfo& image_info(const SkBitmap& sk_bitmap);

  static uint64_t row_bytes(const SkBitmap& sk_bitmap);

  static base::Optional<base::WritableSharedMemoryRegion> pixels(
      const SkBitmap& sk_bitmap);

  static bool Read(viz::mojom::BitmapInSharedMemoryDataView data,
                   SkBitmap* sk_bitmap);
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_BITMAP_IN_SHARED_MEMORY_MOJOM_TRAITS_H_
