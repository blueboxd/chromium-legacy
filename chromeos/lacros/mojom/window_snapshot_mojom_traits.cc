// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/lacros/mojom/window_snapshot_mojom_traits.h"

#include "base/numerics/checked_math.h"

namespace mojo {

// static
bool StructTraits<
    lacros::mojom::WindowSnapshotDataView,
    lacros::WindowSnapshot>::Read(lacros::mojom::WindowSnapshotDataView data,
                                  lacros::WindowSnapshot* out) {
  out->width = data.width();
  out->height = data.height();

  ArrayDataView<uint8_t> bitmap;
  data.GetBitmapDataView(&bitmap);

  uint32_t size;
  size = base::CheckMul(out->width, out->height).ValueOrDie();
  size = base::CheckMul(size, 4).ValueOrDie();

  if (bitmap.size() != base::checked_cast<size_t>(size))
    return false;

  const uint8_t* base = bitmap.data();
  out->bitmap.assign(base, base + bitmap.size());
  return true;
}

}  // namespace mojo
