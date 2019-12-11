// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ostream>

#include "ui/views/layout/layout_types.h"

namespace views {

void PrintTo(const SizeBounds& size_bounds, ::std::ostream* os) {
  *os << size_bounds.ToString();
}

void PrintTo(LayoutOrientation layout_orientation, ::std::ostream* os) {
  switch (layout_orientation) {
    case LayoutOrientation::kHorizontal:
      *os << "LayoutOrientation::kHorizontal";
      break;
    case LayoutOrientation::kVertical:
      *os << "LayoutOrientation::kVertical";
      break;
  }
}

}  // namespace views
