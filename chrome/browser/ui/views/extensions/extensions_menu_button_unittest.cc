// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_button.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/toolbar/test_toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/native_widget_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "ui/events/event.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/test/button_test_api.h"

class ExtensionsMenuButtonTest : public BrowserWithTestWindowTest {
 protected:
  ExtensionsMenuButtonTest()
      : initial_extension_name_(base::ASCIIToUTF16("Initial Extension Name")),
        initial_tooltip_(base::ASCIIToUTF16("Initial tooltip")) {}
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams init_params(
        views::Widget::InitParams::TYPE_POPUP);
    init_params.ownership =
        views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
#if !defined(OS_CHROMEOS) && !defined(OS_MACOSX)
    // This was copied from BookmarkBarViewTest:
    // On Chrome OS, this always creates a NativeWidgetAura, but it should
    // create a DesktopNativeWidgetAura for Mash. We can get by without manually
    // creating it because AshTestViewsDelegate and MusClient will do the right
    // thing automatically.
    init_params.native_widget =
        CreateNativeWidget(NativeWidgetType::DESKTOP_NATIVE_WIDGET_AURA,
                           &init_params, widget_.get());
#endif
    widget_->Init(init_params);

    auto controller =
        std::make_unique<TestToolbarActionViewController>("hello");
    controller_ = controller.get();
    controller_->SetActionName(initial_extension_name_);
    controller_->SetTooltip(initial_tooltip_);
    button_ = std::make_unique<ExtensionsMenuButton>(browser(),
                                                     std::move(controller));
    button_->set_owned_by_client();

    widget_->SetContentsView(button_.get());
  }

  void TearDown() override {
    // All windows need to be closed before tear down.
    widget_.reset();

    BrowserWithTestWindowTest::TearDown();
  }

  void TriggerNotifyClick() {
    ui::MouseEvent click_event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                               base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, 0);
    views::test::ButtonTestApi test_api(button_.get());
    test_api.NotifyClick(click_event);
  }

  const base::string16 initial_extension_name_;
  const base::string16 initial_tooltip_;
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<ExtensionsMenuButton> button_;
  TestToolbarActionViewController* controller_ = nullptr;
};

TEST_F(ExtensionsMenuButtonTest, UpdatesToDisplayCorrectActionTitle) {
  EXPECT_EQ(button_->title()->GetText(), initial_extension_name_);

  base::string16 extension_name = base::ASCIIToUTF16("Extension Name");
  controller_->SetActionName(extension_name);

  EXPECT_EQ(button_->title()->GetText(), extension_name);
}

TEST_F(ExtensionsMenuButtonTest, NotifyClickExecutesAction) {
  EXPECT_EQ(0, controller_->execute_action_count());
  TriggerNotifyClick();
  EXPECT_EQ(1, controller_->execute_action_count());
}

TEST_F(ExtensionsMenuButtonTest, UpdatesToDisplayTooltip) {
  EXPECT_EQ(button_->GetTooltipText(gfx::Point()), initial_tooltip_);

  base::string16 tooltip = base::ASCIIToUTF16("New Tooltip");
  controller_->SetTooltip(tooltip);

  EXPECT_EQ(button_->GetTooltipText(gfx::Point()), tooltip);
}

TEST_F(ExtensionsMenuButtonTest, ButtonMatchesEnabledStateOfExtension) {
  EXPECT_TRUE(button_->GetEnabled());
  controller_->SetEnabled(false);
  EXPECT_FALSE(button_->GetEnabled());
  controller_->SetEnabled(true);
  EXPECT_TRUE(button_->GetEnabled());
}
