// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/gestures/back_gesture/test_back_gesture_contextual_nudge_delegate.h"

#include "ash/public/cpp/back_gesture_contextual_nudge_controller.h"

namespace ash {

TestBackGestureContextualNudgeDelegate::TestBackGestureContextualNudgeDelegate(
    BackGestureContextualNudgeController* controller)
    : controller_(controller) {}

TestBackGestureContextualNudgeDelegate::
    ~TestBackGestureContextualNudgeDelegate() = default;

void TestBackGestureContextualNudgeDelegate::MaybeStartTrackingNavigation(
    aura::Window* window) {
  controller_->NavigationEntryChanged(window);
}

}  // namespace ash
