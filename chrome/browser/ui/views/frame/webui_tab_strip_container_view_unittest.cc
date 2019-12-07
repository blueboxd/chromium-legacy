// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/webui_tab_strip_container_view.h"

#include "base/command_line.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/common/chrome_switches.h"
#include "ui/base/test/material_design_controller_test_api.h"
#include "ui/base/ui_base_switches.h"

class WebUITabStripContainerViewTest : public TestWithBrowserView {
 public:
  template <typename... Args>
  explicit WebUITabStripContainerViewTest(Args... args)
      : TestWithBrowserView(args...), touch_mode_(true) {
    // Both the switch and |touch_mode_| are necessary since
    // MaterialDesignController::Initialize() gets called at different
    // times on different platforms.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kTopChromeTouchUi, switches::kTopChromeTouchUiEnabled);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kWebUITabStrip);
  }

  ~WebUITabStripContainerViewTest() override = default;

 private:
  ui::test::MaterialDesignControllerTestAPI touch_mode_;
};

TEST_F(WebUITabStripContainerViewTest, TabStripStartsClosed) {
  EXPECT_TRUE(WebUITabStripContainerView::UseTouchableTabStrip());
  ASSERT_NE(nullptr, browser_view()->webui_tab_strip());
  EXPECT_FALSE(browser_view()->webui_tab_strip()->GetVisible());
}

TEST_F(WebUITabStripContainerViewTest, TouchModeTransition) {
  EXPECT_TRUE(WebUITabStripContainerView::UseTouchableTabStrip());
  EXPECT_NE(nullptr, browser_view()->webui_tab_strip());
  EXPECT_FALSE(browser_view()->IsTabStripVisible());

  ui::test::MaterialDesignControllerTestAPI disable_touch_mode(false);
  browser_view()->Layout();
  EXPECT_FALSE(WebUITabStripContainerView::UseTouchableTabStrip());
  EXPECT_TRUE(browser_view()->IsTabStripVisible());

  ui::test::MaterialDesignControllerTestAPI reenable_touch_mode(true);
  browser_view()->Layout();
  EXPECT_TRUE(WebUITabStripContainerView::UseTouchableTabStrip());
  EXPECT_FALSE(browser_view()->IsTabStripVisible());
  ASSERT_NE(nullptr, browser_view()->webui_tab_strip());
}

class WebUITabStripDevToolsTest : public WebUITabStripContainerViewTest {
 public:
  WebUITabStripDevToolsTest()
      : WebUITabStripContainerViewTest(Browser::TYPE_DEVTOOLS) {}
  ~WebUITabStripDevToolsTest() override = default;
};

// Regression test for crbug.com/1010247.
TEST_F(WebUITabStripDevToolsTest, DevToolsWindowHasNoTabStrip) {
  EXPECT_EQ(nullptr, browser_view()->webui_tab_strip());

  ui::test::MaterialDesignControllerTestAPI disable_touch_mode(false);
  ui::test::MaterialDesignControllerTestAPI reenable_touch_mode(true);
  EXPECT_EQ(nullptr, browser_view()->webui_tab_strip());
}
