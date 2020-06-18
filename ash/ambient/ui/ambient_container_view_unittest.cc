// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_container_view.h"

#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/test/ambient_ash_test_base.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"

namespace ash {

using AmbientContainerViewTest = AmbientAshTestBase;

// Shows the widget for AmbientContainerView.
TEST_F(AmbientContainerViewTest, Show) {
  // Show the widget.
  ambient_controller()->Toggle();
  AmbientContainerView* ambient_container_view = GetView();
  EXPECT_TRUE(ambient_container_view);

  views::Widget* widget = ambient_container_view->GetWidget();
  EXPECT_TRUE(widget);
}

// Tests that the window can be shown or closed by toggling.
TEST_F(AmbientContainerViewTest, ToggleWindow) {
  // Show the widget.
  ambient_controller()->Toggle();
  EXPECT_TRUE(GetView());

  // Call |Toggle()| to close the widget.
  ambient_controller()->Toggle();
  EXPECT_FALSE(GetView());
}

// Tests that AmbientContainerView window should be fullscreen.
TEST_F(AmbientContainerViewTest, WindowFullscreenSize) {
  // Show the widget.
  ambient_controller()->Toggle();
  views::Widget* widget = GetView()->GetWidget();

  gfx::Rect root_window_bounds =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(widget->GetNativeWindow()->GetRootWindow())
          .bounds();
  gfx::Rect container_window_bounds =
      widget->GetNativeWindow()->GetBoundsInScreen();
  EXPECT_EQ(root_window_bounds, container_window_bounds);
}

}  // namespace ash
