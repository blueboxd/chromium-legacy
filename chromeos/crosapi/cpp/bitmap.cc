// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/crosapi/cpp/bitmap.h"

namespace crosapi {

Bitmap::Bitmap() = default;
Bitmap::Bitmap(Bitmap&& other) = default;
Bitmap& Bitmap::operator=(Bitmap&& other) = default;
Bitmap::~Bitmap() = default;

}  // namespace crosapi
