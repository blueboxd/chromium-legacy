// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/layout_types.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"

#include "ui/gfx/geometry/size.h"

namespace views {

// SizeBounds ------------------------------------------------------------------

SizeBounds::SizeBounds() = default;

SizeBounds::SizeBounds(base::Optional<int> width, base::Optional<int> height)
    : width_(std::move(width)), height_(std::move(height)) {}

SizeBounds::SizeBounds(const SizeBounds& other)
    : width_(other.width()), height_(other.height()) {}

SizeBounds::SizeBounds(const gfx::Size& other)
    : width_(other.width()), height_(other.height()) {}

void SizeBounds::Enlarge(int width, int height) {
  if (width_)
    width_ = std::max(0, *width_ + width);
  if (height_)
    height_ = std::max(0, *height_ + height);
}

bool SizeBounds::operator==(const SizeBounds& other) const {
  return std::tie(width_, height_) == std::tie(other.width_, other.height_);
}

bool SizeBounds::operator!=(const SizeBounds& other) const {
  return !(*this == other);
}

bool SizeBounds::operator<(const SizeBounds& other) const {
  return std::tie(height_, width_) < std::tie(other.height_, other.width_);
}

std::string SizeBounds::ToString() const {
  return base::StrCat({width_ ? base::NumberToString(*width_) : "_", " x ",
                       height_ ? base::NumberToString(*height_) : "_"});
}

}  // namespace views
