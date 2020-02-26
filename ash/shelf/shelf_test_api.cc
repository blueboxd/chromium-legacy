// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shelf_test_api.h"

#include "ash/public/cpp/scrollable_shelf_info.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/scrollable_shelf_view.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"

namespace {

ash::Shelf* GetShelf() {
  return ash::Shell::Get()->GetPrimaryRootWindowController()->shelf();
}

ash::ShelfWidget* GetShelfWidget() {
  return ash::Shell::GetRootWindowControllerWithDisplayId(
             display::Screen::GetScreen()->GetPrimaryDisplay().id())
      ->shelf()
      ->shelf_widget();
}

ash::ScrollableShelfView* GetScrollableShelfView() {
  return GetShelfWidget()->hotseat_widget()->scrollable_shelf_view();
}

}  // namespace

namespace ash {

ShelfTestApi::ShelfTestApi() = default;
ShelfTestApi::~ShelfTestApi() = default;

bool ShelfTestApi::IsVisible() {
  return GetShelf()->shelf_layout_manager()->IsVisible();
}

bool ShelfTestApi::IsAlignmentBottomLocked() {
  return GetShelf()->alignment() == ShelfAlignment::kBottomLocked;
}

views::View* ShelfTestApi::GetHomeButton() {
  return GetShelfWidget()->navigation_widget()->GetHomeButton();
}

ScrollableShelfInfo ShelfTestApi::GetScrollableShelfInfoForState(
    const ScrollableShelfState& state) {
  const auto* scrollable_shelf_view = GetScrollableShelfView();

  ScrollableShelfInfo info;
  info.main_axis_offset =
      scrollable_shelf_view->CalculateMainAxisScrollDistance();
  info.page_offset = scrollable_shelf_view->CalculatePageScrollingOffsetInAbs(
      scrollable_shelf_view->layout_strategy_);
  info.left_arrow_bounds =
      scrollable_shelf_view->left_arrow()->GetBoundsInScreen();
  info.right_arrow_bounds =
      scrollable_shelf_view->right_arrow()->GetBoundsInScreen();
  info.is_animating = scrollable_shelf_view->during_scroll_animation_;

  // Calculates the target offset only when |scroll_distance| is specified.
  if (state.scroll_distance != 0.f) {
    const float target_offset =
        scrollable_shelf_view->CalculateTargetOffsetAfterScroll(
            info.main_axis_offset, state.scroll_distance);
    info.target_main_axis_offset = target_offset;
  }

  return info;
}

}  // namespace ash
