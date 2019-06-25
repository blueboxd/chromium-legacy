// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/immersive/immersive_fullscreen_controller_test_api.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller_test.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/hosted_app_button_container.h"
#include "chrome/browser/ui/views/frame/hosted_app_menu_button.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller_ash.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/interactive_test_utils.h"

class HostedAppAshInteractiveUITest : public extensions::ExtensionBrowserTest {
 public:
  HostedAppAshInteractiveUITest() = default;
  ~HostedAppAshInteractiveUITest() override = default;

  // InProcessBrowserTest override:
  void SetUpOnMainThread() override {
    WebApplicationInfo web_app_info;
    web_app_info.app_url = GURL("https://test.org");
    const extensions::Extension* app = InstallBookmarkApp(web_app_info);

    Browser* browser = ExtensionBrowserTest::LaunchAppBrowser(app);
    browser_view_ = BrowserView::GetBrowserViewForBrowser(browser);

    controller_ = browser_view_->immersive_mode_controller();
    ash::ImmersiveFullscreenControllerTestApi(
        static_cast<ImmersiveModeControllerAsh*>(controller_)->controller())
        .SetupForTest();
    HostedAppButtonContainer::DisableAnimationForTesting();
  }

  void CheckHostedAppMenuClickable() {
    AppMenuButton* menu_button =
        browser_view_->toolbar_button_provider()->GetAppMenuButton();

    // Open the app menu by clicking on it.
    base::RunLoop open_loop;
    ui_test_utils::MoveMouseToCenterAndPress(
        menu_button, ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        open_loop.QuitClosure());
    open_loop.Run();
    EXPECT_TRUE(menu_button->IsMenuShowing());

    // Close the app menu by clicking on it again.
    base::RunLoop close_loop;
    ui_test_utils::MoveMouseToCenterAndPress(
        menu_button, ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        close_loop.QuitClosure());
    close_loop.Run();
    EXPECT_FALSE(menu_button->IsMenuShowing());
  }

  BrowserView* browser_view_ = nullptr;
  ImmersiveModeController* controller_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(HostedAppAshInteractiveUITest);
};

// Test that the hosted app menu button opens a menu on click.
IN_PROC_BROWSER_TEST_F(HostedAppAshInteractiveUITest, MenuButtonClickable) {
  CheckHostedAppMenuClickable();
}

// Test that the hosted app menu button opens a menu on click in immersive mode.
IN_PROC_BROWSER_TEST_F(HostedAppAshInteractiveUITest,
                       ImmersiveMenuButtonClickable) {
  FullscreenNotificationObserver waiter(browser());
  chrome::ToggleFullscreenMode(browser());
  waiter.Wait();

  std::unique_ptr<ImmersiveRevealedLock> revealed_lock(
      controller_->GetRevealedLock(
          ImmersiveModeControllerAsh::ANIMATE_REVEAL_NO));

  CheckHostedAppMenuClickable();
}
