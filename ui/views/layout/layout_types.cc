// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/layout_types.h"

#include <algorithm>

#include "base/strings/stringprintf.h"
#include "ui/gfx/geometry/size.h"

namespace views {

namespace {

std::string OptionalToString(const base::Optional<int>& opt) {
  if (!opt.has_value())
    return "_";
  return base::StringPrintf("%d", opt.value());
}

}  // namespace

// SizeBounds ------------------------------------------------------------------

SizeBounds::SizeBounds() = default;

SizeBounds::SizeBounds(const base::Optional<int>& width,
                       const base::Optional<int>& height)
    : width_(width), height_(height) {}

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
  return width_ == other.width_ && height_ == other.height_;
}

bool SizeBounds::operator!=(const SizeBounds& other) const {
  return !(*this == other);
}

bool SizeBounds::operator<(const SizeBounds& other) const {
  return std::tie(height_, width_) < std::tie(other.height_, other.width_);
}

std::string SizeBounds::ToString() const {
  return base::StringPrintf("%s x %s", OptionalToString(width()).c_str(),
                            OptionalToString(height()).c_str());
}

}  // namespace views
