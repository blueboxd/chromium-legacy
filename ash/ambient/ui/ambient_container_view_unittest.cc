// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_container_view.h"

#include "ash/ambient/ambient_controller.h"
#include "ash/public/cpp/ambient/photo_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_switches.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

class PhotoControllerTest : public PhotoController {
 public:
  PhotoControllerTest() = default;
  ~PhotoControllerTest() override = default;

  // PhotoController:
  void GetNextImage(PhotoController::PhotoDownloadCallback callback) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PhotoControllerTest);
};

}  // namespace

class AmbientContainerViewTest : public AshTestBase {
 public:
  AmbientContainerViewTest() = default;
  ~AmbientContainerViewTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::switches::kAmbientModeFeature);
    photo_controller_ = std::make_unique<PhotoControllerTest>();
    AshTestBase::SetUp();
  }

  void Toggle() { AmbientController()->Toggle(); }

  AmbientContainerView* GetView() {
    return AmbientController()->GetAmbientContainerViewForTesting();
  }

 private:
  AmbientController* AmbientController() {
    return Shell::Get()->ambient_controller();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<PhotoControllerTest> photo_controller_;

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

// Test that the window can be shown or closed by toggling.
TEST_F(AmbientContainerViewTest, ToggleWindow) {
  // Show the widget.
  Toggle();
  EXPECT_TRUE(GetView());

  // Call |Toggle()| to close the widget.
  Toggle();
  EXPECT_FALSE(GetView());
}

// AmbientContainerView window should be fullscreen.
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

}  // namespace ash
