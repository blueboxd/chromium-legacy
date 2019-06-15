// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/browser_shortcut_launcher_item_controller.h"

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"

using BrowserShortcutLauncherItemControllerTest = InProcessBrowserTest;
IN_PROC_BROWSER_TEST_F(BrowserShortcutLauncherItemControllerTest, AppMenu) {
  BrowserShortcutLauncherItemController* controller =
      ChromeLauncherController::instance()
          ->GetBrowserShortcutLauncherItemController();
  ASSERT_TRUE(controller);

  BrowserList* browser_list = BrowserList::GetInstance();
  EXPECT_EQ(browser_list->size(), 1U);

  Browser* browser1 = CreateBrowser(browser()->profile());
  EXPECT_EQ(browser_list->size(), 2U);

  ui_test_utils::NavigateToURL(browser(),
                               GURL("data:text/html,<title>0</title>"));
  ui_test_utils::NavigateToURL(browser1,
                               GURL("data:text/html,<title>1</title>"));
  auto items = controller->GetAppMenuItems(ui::EF_NONE);
  ASSERT_EQ(items.size(), 2U);
  EXPECT_EQ(base::ASCIIToUTF16("0"), items[0].first);
  EXPECT_EQ(base::ASCIIToUTF16("1"), items[1].first);

  // Close the window and wait for all asynchronous window teardown.
  browser1->window()->Close();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(browser_list->size(), 1U);
  // Selecting the app menu item for the closed browser window should not crash.
  controller->ExecuteCommand(/*from_context_menu=*/false, /*command_id=*/1,
                             ui::EF_NONE, display::kInvalidDisplayId);

  // Create and close a window, but don't allow asynchronous teardown to occur.
  browser1 = CreateBrowser(browser()->profile());
  EXPECT_EQ(browser_list->size(), 2U);
  browser1->window()->Close();
  EXPECT_EQ(browser_list->size(), 2U);
  // The app menu should not list the browser window while it is closing.
  items = controller->GetAppMenuItems(ui::EF_NONE);
  EXPECT_EQ(items.size(), 1U);
  // Now, allow the asynchronous teardown to occur.
  EXPECT_EQ(browser_list->size(), 2U);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(browser_list->size(), 1U);
}
