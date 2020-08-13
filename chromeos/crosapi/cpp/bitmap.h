// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CROSAPI_CPP_BITMAP_H_
#define CHROMEOS_CROSAPI_CPP_BITMAP_H_

#include <stdint.h>

#include <vector>

#include "base/component_export.h"

namespace crosapi {

// A 4-byte RGBA bitmap representation. Its size must be exactly equal to
// width * height * 4.
struct COMPONENT_EXPORT(CROSAPI) Bitmap {
  Bitmap();
  ~Bitmap();
  uint32_t width = 0;
  uint32_t height = 0;
  std::vector<uint8_t> pixels;
};

}  // namespace crosapi

#endif  // CHROMEOS_CROSAPI_CPP_BITMAP_H_
