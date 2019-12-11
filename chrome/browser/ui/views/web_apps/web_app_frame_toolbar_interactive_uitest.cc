// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_frame_toolbar_test.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "url/gurl.h"

using WebAppFrameToolbarInteractiveUITest = WebAppFrameToolbarTest;

// Verifies that for minimal-ui web apps, the toolbar keyboard focus cycles
// between the Reload and app menu buttons.
IN_PROC_BROWSER_TEST_F(WebAppFrameToolbarInteractiveUITest, CycleFocus) {
  const GURL app_url("https://test.org");
  InstallAndLaunchWebApp(app_url);

  // Test relies on browser window activation, while platform such as Linux's
  // window activation is asynchronous.
  ui_test_utils::BrowserActivationWaiter waiter(app_browser());
  waiter.WaitForActivation();

  // Send focus to the toolbar as if the user pressed Alt+Shift+T.
  app_browser()->command_controller()->ExecuteCommand(IDC_FOCUS_TOOLBAR);

  // After focusing the toolbar, the reload button should immediately have focus
  // because the back button is disabled (no navigation yet).
  views::FocusManager* const focus_manager = browser_view()->GetFocusManager();
  EXPECT_EQ(focus_manager->GetFocusedView()->GetID(), VIEW_ID_RELOAD_BUTTON);

  // Press Tab to cycle through all of the controls in the toolbar until
  // we end up back where we started.
  // This approach is similar to ToolbarViewTest::RunToolbarCycleFocusTest().
  focus_manager->AdvanceFocus(false);
  EXPECT_EQ(focus_manager->GetFocusedView()->GetID(), VIEW_ID_APP_MENU);
  focus_manager->AdvanceFocus(false);
  EXPECT_EQ(focus_manager->GetFocusedView()->GetID(), VIEW_ID_RELOAD_BUTTON);

  // Now press Shift-Tab to cycle backwards.
  focus_manager->AdvanceFocus(true);
  EXPECT_EQ(focus_manager->GetFocusedView()->GetID(), VIEW_ID_APP_MENU);
  focus_manager->AdvanceFocus(true);
  EXPECT_EQ(focus_manager->GetFocusedView()->GetID(), VIEW_ID_RELOAD_BUTTON);
}
