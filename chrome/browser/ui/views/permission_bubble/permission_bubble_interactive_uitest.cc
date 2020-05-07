// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/permissions/permission_request_manager_test_api.h"
#include "content/public/test/browser_test.h"
#include "ui/base/test/ui_controls.h"
#include "ui/views/test/widget_test.h"

class PermissionBubbleInteractiveUITest : public InProcessBrowserTest {
 public:
  PermissionBubbleInteractiveUITest() = default;

  void EnsureWindowActive(ui::BaseWindow* window, const char* message) {
    EnsureWindowActive(
        views::Widget::GetWidgetForNativeWindow(window->GetNativeWindow()),
        message);
  }

  void EnsureWindowActive(views::Widget* widget, const char* message) {
    SCOPED_TRACE(message);
    EXPECT_TRUE(widget);

    views::test::WidgetActivationWaiter waiter(widget, true);
    waiter.Wait();
  }

  // Send Ctrl/Cmd+keycode in the key window to the browser.
  void SendAccelerator(ui::KeyboardCode keycode, bool shift, bool alt) {
#if defined(OS_MACOSX)
    bool control = false;
    bool command = true;
#else
    bool control = true;
    bool command = false;
#endif
    ui_controls::SendKeyPress(browser()->window()->GetNativeWindow(), keycode,
                              control, shift, alt, command);
  }

  void SetUpOnMainThread() override {
    // Make the browser active (ensures the app can receive key events).
    EXPECT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

    test_api_ =
        std::make_unique<test::PermissionRequestManagerTestApi>(browser());
    EXPECT_TRUE(test_api_->manager());

    test_api_->AddSimpleRequest(ContentSettingsType::GEOLOCATION);

    EXPECT_TRUE(browser()->window()->IsActive());

    // The permission prompt is shown asynchronously.
    base::RunLoop().RunUntilIdle();
    EnsureWindowActive(test_api_->GetPromptWindow(), "show permission bubble");
  }

 protected:
  std::unique_ptr<test::PermissionRequestManagerTestApi> test_api_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PermissionBubbleInteractiveUITest);
};

#if defined(OS_WIN) || defined(OS_LINUX)
// TODO(https://crbug.com/866878): Accelerators are broken when this bubble is
// showing on non-Mac.
#define MAYBE_CmdWClosesWindow DISABLED_CmdWClosesWindow
#else
#define MAYBE_CmdWClosesWindow CmdWClosesWindow
#endif

// There is only one tab. Ctrl/Cmd+w will close it along with the browser
// window.
IN_PROC_BROWSER_TEST_F(PermissionBubbleInteractiveUITest,
                       MAYBE_CmdWClosesWindow) {
  EXPECT_TRUE(browser()->window()->IsVisible());

  SendAccelerator(ui::VKEY_W, false, false);

  // The actual window close happens via a posted task.
  EXPECT_TRUE(browser()->window()->IsVisible());
  ui_test_utils::WaitForBrowserToClose(browser());
  // The window has been destroyed at this point, so there should be no widgets
  // hanging around.
  EXPECT_EQ(0u, views::test::WidgetTest::GetAllWidgets().size());
}

#if defined(OS_WIN) || defined(OS_LINUX)
// TODO(https://crbug.com/866878): Accelerators are broken when this bubble is
// showing on non-Mac.
#define MAYBE_SwitchTabs DISABLED_SwitchTabs
#else
#define MAYBE_SwitchTabs SwitchTabs
#endif

// Add a tab, ensure we can switch away and back using Ctrl/Cmd+Alt+Left/Right
// and curly braces.
IN_PROC_BROWSER_TEST_F(PermissionBubbleInteractiveUITest, MAYBE_SwitchTabs) {
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
  EXPECT_TRUE(test_api_->GetPromptWindow());

  // Add a blank tab in the foreground.
  AddBlankTabAndShow(browser());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  // The bubble should hide and give focus back to the browser. However, the
  // test environment can't guarantee that macOS decides that the Browser window
  // is actually the "best" window to activate upon closing the current key
  // window. So activate it manually.
  browser()->window()->Activate();
  EnsureWindowActive(browser()->window(), "tab added");

  // Prompt is hidden while its tab is not active.
  EXPECT_FALSE(test_api_->GetPromptWindow());

  // Now a webcontents is active, it gets a first shot at processing the
  // accelerator before sending it back unhandled to the browser via IPC. That's
  // all a bit much to handle in a test, so activate the location bar.
  chrome::FocusLocationBar(browser());
  SendAccelerator(ui::VKEY_LEFT, false, true);
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());

  // Note we don't need to makeKeyAndOrderFront: the permission window will take
  // focus when it is shown again.
  EnsureWindowActive(test_api_->GetPromptWindow(),
                     "switched to permission tab with arrow");
  EXPECT_TRUE(test_api_->GetPromptWindow());

  // Ensure we can switch away with the bubble active.
  SendAccelerator(ui::VKEY_RIGHT, false, true);
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  browser()->window()->Activate();
  EnsureWindowActive(browser()->window(), "switch away with arrow");
  EXPECT_FALSE(test_api_->GetPromptWindow());

  // Also test switching tabs with curly braces. "VKEY_OEM_4" is
  // LeftBracket/Brace on a US keyboard, which ui::MacKeyCodeForWindowsKeyCode
  // will map to '{' when shift is passed. Also note there are only two tabs so
  // it doesn't matter which direction is taken (it wraps).
  chrome::FocusLocationBar(browser());
  SendAccelerator(ui::VKEY_OEM_4, true, false);
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
  EnsureWindowActive(test_api_->GetPromptWindow(),
                     "switch to permission tab with curly brace");
  EXPECT_TRUE(test_api_->GetPromptWindow());

  SendAccelerator(ui::VKEY_OEM_4, true, false);
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
  browser()->window()->Activate();
  EnsureWindowActive(browser()->window(), "switch away with curly brace");
  EXPECT_FALSE(test_api_->GetPromptWindow());
}
