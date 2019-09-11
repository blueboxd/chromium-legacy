// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/test/ui_controls.h"

namespace {

const int kSelectHeight = 16;
const int kSelectWidth = 44;
const int kSelectOffsetX = 100;

class PopupOperationsTest : public InProcessBrowserTest {
 public:
  PopupOperationsTest() {}

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // Wait for the active web contents title to match |title|.
  void WaitForTitle(const std::string& title) {
    const base::string16 expected_title(base::ASCIIToUTF16(title));
    content::TitleWatcher title_watcher(GetActiveWebContents(), expected_title);
    ASSERT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }

  void NavigateAndWaitForLoad() {
    ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

    // Navigate to the test page and wait for onload to be called.
    const GURL url = ui_test_utils::GetTestUrl(
        base::FilePath(),
        base::FilePath(FILE_PATH_LITERAL("select_popup.html")));
    ui_test_utils::NavigateToURL(browser(), url);
    WaitForTitle("onload");
  }

  DISALLOW_COPY_AND_ASSIGN(PopupOperationsTest);
};

IN_PROC_BROWSER_TEST_F(PopupOperationsTest, Load) {
  NavigateAndWaitForLoad();
}

#if defined(OS_MACOSX) || defined(OS_CHROMEOS)
// OS_MACOSX: Missing automation provider support: http://crbug.com/1000752.
#define MAYBE_OpenPopup DISABLED_OpenPopup
#else
#define MAYBE_OpenPopup OpenPopup
#endif

// Clicking on a select element should open its popup.
IN_PROC_BROWSER_TEST_F(PopupOperationsTest, MAYBE_OpenPopup) {
  NavigateAndWaitForLoad();

  // Click on the third select to open its popup.
  gfx::Rect bounds = GetActiveWebContents()->GetContainerBounds();
  ui_controls::SendMouseMove(bounds.x() + kSelectWidth / 2 + kSelectOffsetX * 2,
                             bounds.y() + kSelectHeight / 2);
  ui_controls::SendMouseClick(ui_controls::LEFT);
  WaitForTitle("onclick3");
}

#if defined(OS_MACOSX) || defined(OS_CHROMEOS)
// OS_MACOSX: Missing automation provider support: http://crbug.com/1000752.
#define MAYBE_ChangeValue DISABLED_ChangeValue
#else
#define MAYBE_ChangeValue ChangeValue
#endif

// Clicking on a select element should open its popup and move focus to the
// new popup.
IN_PROC_BROWSER_TEST_F(PopupOperationsTest, MAYBE_ChangeValue) {
  NavigateAndWaitForLoad();

  // Open the first popup and change the value by pressing UP and Enter.
  gfx::Rect bounds = GetActiveWebContents()->GetContainerBounds();
  ui_controls::SendMouseMove(bounds.x() + kSelectWidth / 2,
                             bounds.y() + kSelectHeight / 2);
  ui_controls::SendMouseClick(ui_controls::LEFT);
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_UP, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_RETURN, false,
                                              false, false, false));
  WaitForTitle("onchange1");
}

#if defined(OS_MACOSX) || defined(OS_CHROMEOS)
// OS_MACOSX: Missing automation provider support: http://crbug.com/1000752.
#define MAYBE_OpenClosePopup DISABLED_OpenClosePopup
#else
// Temporary disabled due to flakiness: https://crbug.com/1002795.
#define MAYBE_OpenClosePopup DISABLED_OpenClosePopup
#endif

// Clicking on a select element while another select element has its
// popup already open, should open the popup of the clicked select element and
// move the focus to the new popup.
IN_PROC_BROWSER_TEST_F(PopupOperationsTest, MAYBE_OpenClosePopup) {
  NavigateAndWaitForLoad();

  // Open the first popup, click on the second select to open its popup and
  // change its value.
  gfx::Rect bounds = GetActiveWebContents()->GetContainerBounds();
  ui_controls::SendMouseMove(bounds.x() + kSelectWidth / 2,
                             bounds.y() + kSelectHeight / 2);
  ui_controls::SendMouseClick(ui_controls::LEFT);

  ui_controls::SendMouseMove(bounds.x() + kSelectWidth / 2 + kSelectOffsetX,
                             bounds.y() + kSelectHeight / 2);
  ui_controls::SendMouseClick(ui_controls::LEFT);
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_UP, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_RETURN, false,
                                              false, false, false));
  WaitForTitle("onchange2");
}

}  // namespace
