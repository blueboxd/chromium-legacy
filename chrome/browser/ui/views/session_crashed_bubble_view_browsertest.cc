// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/session_crashed_bubble_view.h"

#include <string>

#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/buildflags.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"

class SessionCrashedBubbleViewTest : public DialogBrowserTest {
 public:
  SessionCrashedBubbleViewTest() {}
  ~SessionCrashedBubbleViewTest() override {}

  void ShowUi(const std::string& name) override {
    views::View* anchor_view = BrowserView::GetBrowserViewForBrowser(browser())
                                   ->toolbar_button_provider()
                                   ->GetAppMenuButton();
    crash_bubble_ = new SessionCrashedBubbleView(
        anchor_view, browser(), name == "SessionCrashedBubbleOfferUma");
    views::BubbleDialogDelegateView::CreateBubble(crash_bubble_)->Show();
  }

  void SimulateKeyPress(ui::KeyboardCode key, ui::EventFlags flags) {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());

    ui::KeyEvent press_event(ui::ET_KEY_PRESSED, key, flags);
    if (browser_view->GetFocusManager()->OnKeyEvent(press_event))
      browser_view->OnKeyEvent(&press_event);

    ui::KeyEvent release_event(ui::ET_KEY_RELEASED, key, flags);
    if (browser_view->GetFocusManager()->OnKeyEvent(release_event))
      browser_view->OnKeyEvent(&release_event);
  }

 protected:
  SessionCrashedBubbleView* crash_bubble_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionCrashedBubbleViewTest);
};

IN_PROC_BROWSER_TEST_F(SessionCrashedBubbleViewTest,
                       InvokeUi_SessionCrashedBubble) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(SessionCrashedBubbleViewTest,
                       InvokeUi_SessionCrashedBubbleOfferUma) {
  ShowAndVerifyUi();
}

// TODO(https://crbug.com/1068579): Fails on Windows because the simulated key
// events don't trigger the accelerators.
#if !defined(OS_WIN)
// Regression test for https://crbug.com/1042010, it should be possible to focus
// the bubble with the "focus dialog" hotkey combination (Alt+Shift+A).
// Disabled due to flake: https://crbug.com/1068579
IN_PROC_BROWSER_TEST_F(SessionCrashedBubbleViewTest,
                       DISABLED_CanFocusBubbleWithFocusDialogHotkey) {
  ShowUi("SessionCrashedBubble");

  views::FocusManager* focus_manager = crash_bubble_->GetFocusManager();
  views::View* bubble_focused_view = crash_bubble_->GetInitiallyFocusedView();

  focus_manager->ClearFocus();
  EXPECT_FALSE(bubble_focused_view->HasFocus());

  SimulateKeyPress(ui::VKEY_A, static_cast<ui::EventFlags>(ui::EF_ALT_DOWN |
                                                           ui::EF_SHIFT_DOWN));
  EXPECT_TRUE(bubble_focused_view->HasFocus());
}

// Regression test for https://crbug.com/1042010, it should be possible to focus
// the bubble with the "rotate pane focus" (F6) hotkey.
// Disabled due to flake: https://crbug.com/1068579
IN_PROC_BROWSER_TEST_F(SessionCrashedBubbleViewTest,
                       DISABLED_CanFocusBubbleWithRotatePaneFocusHotkey) {
  ShowUi("SessionCrashedBubble");
  views::FocusManager* focus_manager = crash_bubble_->GetFocusManager();
  views::View* bubble_focused_view = crash_bubble_->GetInitiallyFocusedView();

  focus_manager->ClearFocus();
  EXPECT_FALSE(bubble_focused_view->HasFocus());

  SimulateKeyPress(ui::VKEY_F6, ui::EF_NONE);
  // Rotate pane focus is expected to keep the bubble focused until the user
  // deals with it, so a second call should have no effect.
  SimulateKeyPress(ui::VKEY_F6, ui::EF_NONE);
  EXPECT_TRUE(bubble_focused_view->HasFocus());
}
#endif
