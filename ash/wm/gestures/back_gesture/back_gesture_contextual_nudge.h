// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_GESTURES_BACK_GESTURE_BACK_GESTURE_CONTEXTUAL_NUDGE_H_
#define ASH_WM_GESTURES_BACK_GESTURE_BACK_GESTURE_CONTEXTUAL_NUDGE_H_

#include <memory>

#include "ui/views/widget/widget.h"

namespace ash {

class BackGestureContextualNudge {
 public:
  BackGestureContextualNudge();
  ~BackGestureContextualNudge();

 private:
  std::unique_ptr<views::Widget> widget_;

  BackGestureContextualNudge(const BackGestureContextualNudge&) = delete;
  BackGestureContextualNudge& operator=(const BackGestureContextualNudge&) =
      delete;
};

}  // namespace ash

#endif  // ASH_WM_GESTURES_BACK_GESTURE_BACK_GESTURE_CONTEXTUAL_NUDGE_H_
