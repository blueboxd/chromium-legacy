// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/layout_types.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"

namespace views {

// SizeBounds ------------------------------------------------------------------

void SizeBounds::Enlarge(int width, int height) {
  if (width_)
    width_ = std::max(0, *width_ + width);
  if (height_)
    height_ = std::max(0, *height_ + height);
}

std::string SizeBounds::ToString() const {
  return base::StrCat({width_ ? base::NumberToString(*width_) : "_", " x ",
                       height_ ? base::NumberToString(*height_) : "_"});
}

}  // namespace views
