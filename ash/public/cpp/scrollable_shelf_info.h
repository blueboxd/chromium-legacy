// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SCROLLABLE_SHELF_INFO_H_
#define ASH_PUBLIC_CPP_SCROLLABLE_SHELF_INFO_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

// Information of scrollable shelf.
struct ASH_PUBLIC_EXPORT ScrollableShelfInfo {
  // Current offset on the main axis.
  float main_axis_offset = 0.f;

  // Offset required to scroll to the next shelf page.
  float page_offset = 0.f;

  // Target offset on the main axis.
  float target_main_axis_offset = 0.f;

  // Bounds of the left arrow in screen.
  gfx::Rect left_arrow_bounds;

  // Bounds of the right arrow in screen.
  gfx::Rect right_arrow_bounds;

  // Indicates whether scrollable shelf is animating.
  bool is_animating = false;
};

struct ASH_PUBLIC_EXPORT ScrollableShelfState {
  // The distance by which shelf will scroll.
  float scroll_distance = 0.f;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SCROLLABLE_SHELF_INFO_H_
