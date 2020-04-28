// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_container_view.h"

#include <memory>

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/test/ambient_ash_test_base.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/timer/timer.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/widget/widget.h"

namespace ash {

class AmbientContainerViewTest : public AmbientAshTestBase {
 public:
  AmbientContainerViewTest() = default;
  ~AmbientContainerViewTest() override = default;

  AmbientContainerView* GetView() {
    return ambient_controller()->get_container_view_for_testing();
  }

  const base::OneShotTimer& GetTimer() {
    return ambient_controller()->get_timer_for_testing();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AmbientContainerViewTest);
};

// Shows the widget for AmbientContainerView.
TEST_F(AmbientContainerViewTest, Show) {
  // Show the widget.
  Toggle();
  AmbientContainerView* ambient_container_view = GetView();
  EXPECT_TRUE(ambient_container_view);

  views::Widget* widget = ambient_container_view->GetWidget();
  EXPECT_TRUE(widget);
}

// Tests that the window can be shown or closed by toggling.
TEST_F(AmbientContainerViewTest, ToggleWindow) {
  // Show the widget.
  Toggle();
  EXPECT_TRUE(GetView());

  // Call |Toggle()| to close the widget.
  Toggle();
  EXPECT_FALSE(GetView());
}

// Tests that AmbientContainerView window should be fullscreen.
TEST_F(AmbientContainerViewTest, WindowFullscreenSize) {
  // Show the widget.
  Toggle();
  views::Widget* widget = GetView()->GetWidget();

  gfx::Rect root_window_bounds =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(widget->GetNativeWindow()->GetRootWindow())
          .bounds();
  gfx::Rect container_window_bounds =
      widget->GetNativeWindow()->GetBoundsInScreen();
  EXPECT_EQ(root_window_bounds, container_window_bounds);
}

// Tests that the timer is running on showing and stopped on closing.
TEST_F(AmbientContainerViewTest, TimerRunningWhenShowing) {
  // Show the widget.
  Toggle();
  EXPECT_TRUE(GetView());

  // Download |kImageBufferLength| / 2 + 1 images to fill buffer in PhotoModel,
  // in order to return false in |ShouldFetchImmediately()| and start timer.
  const int num_image_to_load = kImageBufferLength / 2 + 1;
  task_environment()->FastForwardBy(kAnimationDuration * num_image_to_load);

  EXPECT_TRUE(GetTimer().IsRunning());

  // Call |Toggle()| to close the widget.
  Toggle();
  EXPECT_FALSE(GetView());
  EXPECT_FALSE(GetTimer().IsRunning());
}

}  // namespace ash
