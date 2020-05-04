// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/assistive_window_controller.h"

#include "chrome/test/base/chrome_ash_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/wm/core/window_util.h"

namespace chromeos {
namespace input_method {

class AssistiveWindowControllerTest : public ChromeAshTestBase {
 protected:
  AssistiveWindowControllerTest() { ui::IMEBridge::Initialize(); }
  ~AssistiveWindowControllerTest() override { ui::IMEBridge::Shutdown(); }

  void SetUp() override {
    controller_ = std::make_unique<AssistiveWindowController>();
    ui::IMEBridge::Get()->SetAssistiveWindowHandler(controller_.get());

    ChromeAshTestBase::SetUp();
    std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(1));
    wm::ActivateWindow(window.get());
  }

  std::unique_ptr<AssistiveWindowController> controller_;
  const base::string16 suggestion_ = base::UTF8ToUTF16("test");

  void TearDown() override {
    controller_.reset();
    ChromeAshTestBase::TearDown();
  }
};

TEST_F(AssistiveWindowControllerTest, ConfirmedLength0SetsSuggestionViewBound) {
  // Sets up suggestion_view with confirmed_length = 0.
  ui::IMEBridge::Get()->GetAssistiveWindowHandler()->ShowSuggestion(suggestion_,
                                                                    0, false);
  ui::ime::SuggestionWindowView* suggestion_view =
      controller_->GetSuggestionWindowViewForTesting();
  EXPECT_EQ(
      0u,
      ui::IMEBridge::Get()->GetAssistiveWindowHandler()->GetConfirmedLength());

  gfx::Rect current_bounds = suggestion_view->GetAnchorRect();
  gfx::Rect new_bounds(current_bounds.width() + 1, current_bounds.height());
  ui::IMEBridge::Get()->GetAssistiveWindowHandler()->SetBounds(new_bounds);
  EXPECT_EQ(new_bounds, suggestion_view->GetAnchorRect());
}

TEST_F(AssistiveWindowControllerTest,
       ConfirmedLengthNot0DoesNotSetSuggestionViewBound) {
  // Sets up suggestion_view with confirmed_length = 1.
  ui::IMEBridge::Get()->GetAssistiveWindowHandler()->ShowSuggestion(suggestion_,
                                                                    1, false);
  ui::ime::SuggestionWindowView* suggestion_view =
      controller_->GetSuggestionWindowViewForTesting();
  EXPECT_EQ(
      1u,
      ui::IMEBridge::Get()->GetAssistiveWindowHandler()->GetConfirmedLength());

  gfx::Rect current_bounds = suggestion_view->GetAnchorRect();
  gfx::Rect new_bounds(current_bounds.width() + 1, current_bounds.height());
  ui::IMEBridge::Get()->GetAssistiveWindowHandler()->SetBounds(new_bounds);
  EXPECT_EQ(current_bounds, suggestion_view->GetAnchorRect());
}

}  // namespace input_method
}  // namespace chromeos
