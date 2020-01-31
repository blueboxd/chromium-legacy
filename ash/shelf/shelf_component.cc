// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_component.h"

namespace ash {

// TODO(manucornet): Make this pure virtual when all subclasses implement it.
gfx::Rect ShelfComponent::GetTargetBounds() const {
  return gfx::Rect();
}

}  // namespace ash
