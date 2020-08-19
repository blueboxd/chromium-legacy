// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/crosapi/cpp/bitmap_util.h"

#include <algorithm>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "chromeos/crosapi/cpp/bitmap.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"

namespace crosapi {

Bitmap BitmapFromSkBitmap(const SkBitmap& sk_bitmap) {
  int size;
  size = base::CheckMul(sk_bitmap.width(), sk_bitmap.height()).ValueOrDie();
  size = base::CheckMul(size, 4).ValueOrDie();
  CHECK_EQ(sk_bitmap.computeByteSize(), base::checked_cast<size_t>(size));

  uint8_t* base = static_cast<uint8_t*>(sk_bitmap.getPixels());
  std::vector<uint8_t> bytes(base, base + sk_bitmap.computeByteSize());

  crosapi::Bitmap snapshot;
  snapshot.width = sk_bitmap.width();
  snapshot.height = sk_bitmap.height();
  snapshot.pixels.swap(bytes);
  return snapshot;
}

SkBitmap SkBitmapFromBitmap(const Bitmap& snapshot) {
  SkImageInfo info =
      SkImageInfo::Make(snapshot.width, snapshot.height, kBGRA_8888_SkColorType,
                        kPremul_SkAlphaType);
  CHECK_EQ(info.computeByteSize(info.minRowBytes()), snapshot.pixels.size());
  SkBitmap sk_bitmap;
  CHECK(sk_bitmap.tryAllocPixels(info));
  std::copy(snapshot.pixels.begin(), snapshot.pixels.end(),
            static_cast<uint8_t*>(sk_bitmap.getPixels()));
  sk_bitmap.notifyPixelsChanged();
  return sk_bitmap;
}

}  // namespace crosapi
