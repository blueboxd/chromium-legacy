// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_bar_view.h"
#include "ash/capture_mode/capture_mode_close_button.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/capture_mode/capture_mode_source_view.h"
#include "ash/capture_mode/capture_mode_toggle_button.h"
#include "ash/capture_mode/capture_mode_type_view.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/views/view.h"

namespace ash {

namespace {

void ClickOnView(const views::View* view,
                 ui::test::EventGenerator* event_generator) {
  DCHECK(view);
  DCHECK(event_generator);

  const gfx::Point view_center = view->GetBoundsInScreen().CenterPoint();
  event_generator->MoveMouseTo(view_center);
  event_generator->ClickLeftButton();
}

}  // namespace

class CaptureModeTest : public AshTestBase {
 public:
  CaptureModeTest() = default;
  CaptureModeTest(const CaptureModeTest&) = delete;
  CaptureModeTest& operator=(const CaptureModeTest&) = delete;
  ~CaptureModeTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kCaptureMode);
    AshTestBase::SetUp();
  }

  CaptureModeToggleButton* GetImageToggleButton() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    return controller->capture_mode_session()
        ->capture_mode_bar_view()
        ->capture_type_view()
        ->image_toggle_button();
  }

  CaptureModeToggleButton* GetVideoToggleButton() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    return controller->capture_mode_session()
        ->capture_mode_bar_view()
        ->capture_type_view()
        ->video_toggle_button();
  }

  CaptureModeToggleButton* GetFullscreenToggleButton() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    return controller->capture_mode_session()
        ->capture_mode_bar_view()
        ->capture_source_view()
        ->fullscreen_toggle_button();
  }

  CaptureModeToggleButton* GetRegionToggleButton() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    return controller->capture_mode_session()
        ->capture_mode_bar_view()
        ->capture_source_view()
        ->region_toggle_button();
  }

  CaptureModeToggleButton* GetWindowToggleButton() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    return controller->capture_mode_session()
        ->capture_mode_bar_view()
        ->capture_source_view()
        ->window_toggle_button();
  }

  CaptureModeCloseButton* GetCloseButton() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    return controller->capture_mode_session()
        ->capture_mode_bar_view()
        ->close_button();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(CaptureModeTest, StartStop) {
  auto* controller = CaptureModeController::Get();
  controller->Start();
  EXPECT_TRUE(controller->IsActive());
  // Calling start again is a no-op.
  controller->Start();
  EXPECT_TRUE(controller->IsActive());
  controller->Stop();
  EXPECT_FALSE(controller->IsActive());
}

TEST_F(CaptureModeTest, StartWithMostRecentTypeAndSource) {
  auto* controller = CaptureModeController::Get();
  controller->SetSource(CaptureModeSource::kFullscreen);
  controller->SetType(CaptureModeType::kVideo);
  controller->Start();
  EXPECT_TRUE(controller->IsActive());

  EXPECT_FALSE(GetImageToggleButton()->toggled());
  EXPECT_TRUE(GetVideoToggleButton()->toggled());
  EXPECT_TRUE(GetFullscreenToggleButton()->toggled());
  EXPECT_FALSE(GetRegionToggleButton()->toggled());
  EXPECT_FALSE(GetWindowToggleButton()->toggled());

  ClickOnView(GetCloseButton(), GetEventGenerator());
  EXPECT_FALSE(controller->IsActive());
}

TEST_F(CaptureModeTest, ChangeTypeAndSourceFromUI) {
  auto* controller = CaptureModeController::Get();
  controller->Start();
  EXPECT_TRUE(controller->IsActive());

  EXPECT_TRUE(GetImageToggleButton()->toggled());
  EXPECT_FALSE(GetVideoToggleButton()->toggled());
  auto* event_generator = GetEventGenerator();
  ClickOnView(GetVideoToggleButton(), event_generator);
  EXPECT_FALSE(GetImageToggleButton()->toggled());
  EXPECT_TRUE(GetVideoToggleButton()->toggled());
  EXPECT_EQ(controller->type(), CaptureModeType::kVideo);

  ClickOnView(GetWindowToggleButton(), event_generator);
  EXPECT_FALSE(GetFullscreenToggleButton()->toggled());
  EXPECT_FALSE(GetRegionToggleButton()->toggled());
  EXPECT_TRUE(GetWindowToggleButton()->toggled());
  EXPECT_EQ(controller->source(), CaptureModeSource::kWindow);

  ClickOnView(GetFullscreenToggleButton(), event_generator);
  EXPECT_TRUE(GetFullscreenToggleButton()->toggled());
  EXPECT_FALSE(GetRegionToggleButton()->toggled());
  EXPECT_FALSE(GetWindowToggleButton()->toggled());
  EXPECT_EQ(controller->source(), CaptureModeSource::kFullscreen);
}

}  // namespace ash
