// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_COMPONENT_H_
#define ASH_SHELF_SHELF_COMPONENT_H_

#include "ash/ash_export.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

// An interface describing any shelf component such as the navigation widget,
// the hotseat widget or the status area widget, to make it easier to
// coordinate animations for all of them.
class ASH_EXPORT ShelfComponent {
  // Makes the component calculate its new target bounds given the current
  // target conditions. It is the component's responsibility to store the
  // calculated bounds.
  virtual void CalculateTargetBounds() = 0;

  // Returns this component's current target bounds, in screen coordinates.
  virtual gfx::Rect GetTargetBounds() const;

  // Updates the component's layout and bounds to match the most recently
  // calculated target bounds. The change should be animated if |animate| is
  // true.
  virtual void UpdateLayout(bool animate) = 0;
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_COMPONENT_H_
