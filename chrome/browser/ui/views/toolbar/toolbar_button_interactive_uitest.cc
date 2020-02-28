// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_button.h"

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/test/view_event_test_base.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view_class_properties.h"

namespace test {

// Friend of ToolbarButton to access private members.
class ToolbarButtonTestApi {
 public:
  explicit ToolbarButtonTestApi(ToolbarButton* button) : button_(button) {}
  ToolbarButtonTestApi(const ToolbarButtonTestApi&) = delete;
  ToolbarButtonTestApi& operator=(const ToolbarButtonTestApi&) = delete;

  views::MenuRunner* menu_runner() { return button_->menu_runner_.get(); }
  bool menu_showing() const { return button_->menu_showing_; }
  const gfx::Insets last_paint_insets() const {
    return button_->last_paint_insets_;
  }
  const gfx::Insets layout_inset_delta() const {
    return button_->layout_inset_delta_;
  }
  const base::Optional<SkColor> last_border_color() const {
    return button_->last_border_color_;
  }
  void SetAnimationTimingForTesting() {
    button_->highlight_color_animation_.highlight_color_animation_
        .SetSlideDuration(base::TimeDelta());
  }

 private:
  ToolbarButton* button_;
};

}  // namespace test

class TestToolbarButton : public ToolbarButton {
 public:
  using ToolbarButton::ToolbarButton;

  void ResetBorderUpdateFlag() { did_border_update_ = false; }
  bool did_border_update() const { return did_border_update_; }

  // views::ToolbarButton
  void SetBorder(std::unique_ptr<views::Border> b) override {
    ToolbarButton::SetBorder(std::move(b));
    did_border_update_ = true;
  }

 private:
  bool did_border_update_ = false;
};

class ToolbarButtonUITest : public ViewEventTestBase {
 public:
  ToolbarButtonUITest() = default;
  ToolbarButtonUITest(const ToolbarButtonUITest&) = delete;
  ToolbarButtonUITest& operator=(const ToolbarButtonUITest&) = delete;

  // ViewEventTestBase:
  views::View* CreateContentsView() override {
    // Usually a BackForwardMenuModel is used, but that needs a Browser*. Make
    // something simple with at least one item so a menu gets shown. Note that
    // ToolbarButton takes ownership of the |model|.
    auto model = std::make_unique<ui::SimpleMenuModel>(nullptr);
    model->AddItem(0, base::string16());
    button_ = new TestToolbarButton(nullptr, std::move(model), nullptr);
    return button_;
  }
  void DoTestOnMessageLoop() override {}

 protected:
  TestToolbarButton* button_ = nullptr;
};

// Test showing and dismissing a menu to verify menu delegate lifetime.
TEST_F(ToolbarButtonUITest, ShowMenu) {
  test::ToolbarButtonTestApi test_api(button_);

  EXPECT_FALSE(test_api.menu_showing());
  EXPECT_FALSE(test_api.menu_runner());
  EXPECT_EQ(views::Button::STATE_NORMAL, button_->state());

  // Show the menu. Note that it is asynchronous.
  button_->ShowContextMenuForView(nullptr, gfx::Point(), ui::MENU_SOURCE_MOUSE);

  EXPECT_TRUE(test_api.menu_showing());
  EXPECT_TRUE(test_api.menu_runner());
  EXPECT_TRUE(test_api.menu_runner()->IsRunning());

  // Button should appear pressed when the menu is showing.
  EXPECT_EQ(views::Button::STATE_PRESSED, button_->state());

  test_api.menu_runner()->Cancel();

  // Ensure the ToolbarButton's |menu_runner_| member is reset to null.
  EXPECT_FALSE(test_api.menu_showing());
  EXPECT_FALSE(test_api.menu_runner());
  EXPECT_EQ(views::Button::STATE_NORMAL, button_->state());
}

// Test deleting a ToolbarButton while its menu is showing.
TEST_F(ToolbarButtonUITest, DeleteWithMenu) {
  button_->ShowContextMenuForView(nullptr, gfx::Point(), ui::MENU_SOURCE_MOUSE);
  EXPECT_TRUE(test::ToolbarButtonTestApi(button_).menu_runner());
  delete button_;
}

// Tests to make sure the button's border is updated as its height changes.
TEST_F(ToolbarButtonUITest, TestBorderUpdateHeightChange) {
  const gfx::Insets toolbar_padding = GetLayoutInsets(TOOLBAR_BUTTON);

  button_->ResetBorderUpdateFlag();
  for (int bounds_height : {8, 12, 20}) {
    EXPECT_FALSE(button_->did_border_update());
    button_->SetBoundsRect({bounds_height, bounds_height});
    EXPECT_TRUE(button_->did_border_update());
    EXPECT_EQ(button_->border()->GetInsets(), gfx::Insets(toolbar_padding));
    button_->ResetBorderUpdateFlag();
  }
}

// Tests to make sure the button's border color is updated as its animation
// color changes.
TEST_F(ToolbarButtonUITest, TestBorderUpdateColorChange) {
  test::ToolbarButtonTestApi test_api(button_);
  test_api.SetAnimationTimingForTesting();

  button_->ResetBorderUpdateFlag();
  for (SkColor border_color : {SK_ColorRED, SK_ColorGREEN, SK_ColorBLUE}) {
    EXPECT_FALSE(button_->did_border_update());
    button_->SetHighlight(base::string16(), border_color);
    EXPECT_EQ(button_->border()->color(), border_color);
    EXPECT_TRUE(button_->did_border_update());
    button_->ResetBorderUpdateFlag();
  }
}
