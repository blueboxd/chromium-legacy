// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_HIT_THE_WALL_DESK_SWITCH_ANIMATION_H_
#define ASH_WM_DESKS_HIT_THE_WALL_DESK_SWITCH_ANIMATION_H_

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// Animates the given |root| using a bounce-like animation to indicate that
// there are no more desks in the direction the user is trying to go to.
// |going_left| is true when the user requests to switch to a desk on the left
// of the currently active one.
void PerformHitTheWallDeskSwitchAnimation(aura::Window* root, bool going_left);

}  // namespace ash

#endif  // ASH_WM_DESKS_HIT_THE_WALL_DESK_SWITCH_ANIMATION_H_
