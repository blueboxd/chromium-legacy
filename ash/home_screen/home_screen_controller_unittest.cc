// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/home_screen/home_screen_controller.h"

#include <memory>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "base/macros.h"

namespace ash {
namespace {

class HomeScreenControllerTest : public AshTestBase {
 public:
  HomeScreenControllerTest() = default;
  ~HomeScreenControllerTest() override = default;

  std::unique_ptr<aura::Window> CreateTestWindow() {
    return AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  }

  std::unique_ptr<aura::Window> CreatePopupTestWindow() {
    return AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400),
                                         aura::client::WINDOW_TYPE_POPUP);
  }

  HomeScreenController* home_screen_controller() {
    return Shell::Get()->home_screen_controller();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HomeScreenControllerTest);
};

TEST_F(HomeScreenControllerTest, OnlyMinimizeCycleListWindows) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreatePopupTestWindow());

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  std::unique_ptr<ui::Event> test_event = std::make_unique<ui::KeyEvent>(
      ui::EventType::ET_MOUSE_PRESSED, ui::VKEY_UNKNOWN, ui::EF_NONE);
  home_screen_controller()->GoHome(GetPrimaryDisplay().id());
  ASSERT_TRUE(WindowState::Get(w1.get())->IsMinimized());
  ASSERT_FALSE(WindowState::Get(w2.get())->IsMinimized());
}

}  // namespace
}  // namespace ash
