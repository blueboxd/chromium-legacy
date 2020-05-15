// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/chrome_native_app_window_views_aura_ash.h"

#include "ash/public/cpp/immersive/immersive_fullscreen_controller.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/window_properties.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "build/build_config.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/apps/platform_apps/app_window_interactive_uitest_base.h"
#include "chrome/browser/ui/ash/tablet_mode_page_behavior.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/login/login_state/scoped_test_public_session_login_state.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/test/extension_test_message_listener.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/views/view_observer.h"
#include "ui/wm/core/window_util.h"

namespace {

class ViewBoundsChangeWaiter : public views::ViewObserver {
 public:
  static void VerifyY(views::View* view, int y) {
    if (y != view->bounds().y())
      ViewBoundsChangeWaiter(view).run_loop_.Run();

    EXPECT_EQ(y, view->bounds().y());
  }

 private:
  explicit ViewBoundsChangeWaiter(views::View* view) { observed_.Add(view); }
  ~ViewBoundsChangeWaiter() override = default;

  // ViewObserver:
  void OnViewBoundsChanged(views::View* view) override { run_loop_.Quit(); }

  base::RunLoop run_loop_;

  ScopedObserver<views::View, views::ViewObserver> observed_{this};

  DISALLOW_COPY_AND_ASSIGN(ViewBoundsChangeWaiter);
};

}  // namespace

class ChromeNativeAppWindowViewsAuraAshBrowserTest
    : public AppWindowInteractiveTest {
 public:
  ChromeNativeAppWindowViewsAuraAshBrowserTest() = default;
  ~ChromeNativeAppWindowViewsAuraAshBrowserTest() override = default;

 protected:
  void InitWindow() { app_window_ = CreateTestAppWindow("{}"); }

  bool IsImmersiveActive() {
    return window()->widget()->GetNativeWindow()->GetProperty(
        ash::kImmersiveIsActive);
  }

  ChromeNativeAppWindowViewsAuraAsh* window() {
    return static_cast<ChromeNativeAppWindowViewsAuraAsh*>(
        GetFirstAppWindow()->GetBaseWindow());
  }

  extensions::AppWindow* app_window_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeNativeAppWindowViewsAuraAshBrowserTest);
};

// Verify that immersive mode is enabled or disabled as expected.
IN_PROC_BROWSER_TEST_F(ChromeNativeAppWindowViewsAuraAshBrowserTest,
                       ImmersiveWorkFlow) {
  InitWindow();
  ASSERT_TRUE(window());
  EXPECT_FALSE(IsImmersiveActive());
  constexpr int kFrameHeight = 32;

  views::ClientView* client_view =
      window()->widget()->non_client_view()->client_view();
  EXPECT_EQ(kFrameHeight, client_view->bounds().y());

  // Verify that when fullscreen is toggled on, immersive mode is enabled and
  // that when fullscreen is toggled off, immersive mode is disabled.
  app_window_->OSFullscreen();
  EXPECT_TRUE(IsImmersiveActive());
  ViewBoundsChangeWaiter::VerifyY(client_view, 0);

  app_window_->Restore();
  EXPECT_FALSE(IsImmersiveActive());
  ViewBoundsChangeWaiter::VerifyY(client_view, kFrameHeight);

  // Verify that since the auto hide title bars in tablet mode feature turned
  // on, immersive mode is enabled once tablet mode is entered, and disabled
  // once tablet mode is exited.
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);
  EXPECT_TRUE(IsImmersiveActive());
  ViewBoundsChangeWaiter::VerifyY(client_view, 0);

  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  EXPECT_FALSE(IsImmersiveActive());
  ViewBoundsChangeWaiter::VerifyY(client_view, kFrameHeight);

  // Verify that the window was fullscreened before entering tablet mode, it
  // will remain fullscreened after exiting tablet mode.
  app_window_->OSFullscreen();
  EXPECT_TRUE(IsImmersiveActive());
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);
  EXPECT_TRUE(IsImmersiveActive());
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  EXPECT_TRUE(IsImmersiveActive());
  app_window_->Restore();

  // Verify that minimized windows do not have immersive mode enabled.
  app_window_->Minimize();
  EXPECT_FALSE(IsImmersiveActive());
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);
  EXPECT_FALSE(IsImmersiveActive());
  window()->Restore();
  EXPECT_TRUE(IsImmersiveActive());
  app_window_->Minimize();
  EXPECT_FALSE(IsImmersiveActive());
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  EXPECT_FALSE(IsImmersiveActive());

  // Verify that activation change should not change the immersive
  // state.
  window()->Show();
  app_window_->OSFullscreen();
  EXPECT_TRUE(IsImmersiveActive());
  ::wm::DeactivateWindow(window()->GetNativeWindow());
  EXPECT_TRUE(IsImmersiveActive());
  ::wm::ActivateWindow(window()->GetNativeWindow());
  EXPECT_TRUE(IsImmersiveActive());

  CloseAppWindow(app_window_);
}

// Verifies that apps in immersive fullscreen will have a restore state of
// maximized.
IN_PROC_BROWSER_TEST_F(ChromeNativeAppWindowViewsAuraAshBrowserTest,
                       ImmersiveModeFullscreenRestoreType) {
  InitWindow();
  ASSERT_TRUE(window());

  app_window_->OSFullscreen();
  EXPECT_EQ(ui::SHOW_STATE_MAXIMIZED, window()->GetRestoredState());
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);
  EXPECT_TRUE(window()->IsFullscreen());
  EXPECT_EQ(ui::SHOW_STATE_MAXIMIZED, window()->GetRestoredState());
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  EXPECT_EQ(ui::SHOW_STATE_MAXIMIZED, window()->GetRestoredState());

  CloseAppWindow(app_window_);
}

// Verify that immersive mode stays disabled when entering tablet mode in
// forced fullscreen mode (e.g. when running in a kiosk session).
IN_PROC_BROWSER_TEST_F(ChromeNativeAppWindowViewsAuraAshBrowserTest,
                       NoImmersiveModeWhenForcedFullscreen) {
  InitWindow();
  ASSERT_TRUE(window());

  app_window_->ForcedFullscreen();

  ash::ShellTestApi().SetTabletModeEnabledForTest(true);
  EXPECT_FALSE(IsImmersiveActive());
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  EXPECT_FALSE(IsImmersiveActive());
}

// Make sure a normal window is not in immersive mode, and uses
// immersive in fullscreen.
IN_PROC_BROWSER_TEST_F(ChromeNativeAppWindowViewsAuraAshBrowserTest,
                       PublicSessionImmersiveMode) {
  chromeos::ScopedTestPublicSessionLoginState login_state;

  InitWindow();
  ASSERT_TRUE(window());
  EXPECT_FALSE(IsImmersiveActive());

  app_window_->SetFullscreen(extensions::AppWindow::FULLSCREEN_TYPE_HTML_API,
                             true);

  EXPECT_TRUE(IsImmersiveActive());
}

// Verifies that apps in clamshell mode with immersive fullscreen enabled will
// correctly exit immersive mode if exit fullscreen.
IN_PROC_BROWSER_TEST_F(ChromeNativeAppWindowViewsAuraAshBrowserTest,
                       RestoreImmersiveMode) {
  InitWindow();
  ASSERT_TRUE(window());

  // Should not disable immersive fullscreen in tablet mode if |window| exits
  // fullscreen.
  EXPECT_FALSE(window()->IsFullscreen());
  app_window_->OSFullscreen();
  EXPECT_EQ(ui::SHOW_STATE_MAXIMIZED, window()->GetRestoredState());
  EXPECT_TRUE(window()->IsFullscreen());
  EXPECT_TRUE(IsImmersiveActive());
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);
  EXPECT_TRUE(window()->IsFullscreen());
  EXPECT_EQ(ui::SHOW_STATE_MAXIMIZED, window()->GetRestoredState());

  window()->Restore();
  // Restoring a window inside tablet mode should deactivate fullscreen, but not
  // disable immersive mode.
  EXPECT_FALSE(window()->IsFullscreen());
  ASSERT_TRUE(IsImmersiveActive());

  // Immersive fullscreen should be disabled if window exits fullscreen in
  // clamshell mode.
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  app_window_->OSFullscreen();
  EXPECT_EQ(ui::SHOW_STATE_MAXIMIZED, window()->GetRestoredState());
  EXPECT_TRUE(window()->IsFullscreen());

  window()->Restore();
  EXPECT_FALSE(IsImmersiveActive());

  CloseAppWindow(app_window_);
}

IN_PROC_BROWSER_TEST_F(ChromeNativeAppWindowViewsAuraAshBrowserTest,
                       NoImmersiveOrBubbleOutsidePublicSessionWindow) {
  ExtensionTestMessageListener launched_listener("Launched", true);
  LoadAndLaunchPlatformApp("leave_fullscreen", &launched_listener);

  // We start by making sure the window is actually focused.
  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
      GetFirstAppWindow()->GetNativeWindow()));

  // When receiving the reply, the application will try to go fullscreen using
  // the Window API but there is no synchronous way to know if that actually
  // succeeded. Also, failure will not be notified. A failure case will only be
  // known with a timeout.
  {
    FullscreenChangeWaiter fs_changed(GetFirstAppWindow()->GetBaseWindow());

    launched_listener.Reply("window");

    fs_changed.Wait();
  }

  EXPECT_FALSE(window()->IsImmersiveModeEnabled());
  EXPECT_FALSE(window()->exclusive_access_bubble_);
}

IN_PROC_BROWSER_TEST_F(ChromeNativeAppWindowViewsAuraAshBrowserTest,
                       NoImmersiveOrBubbleOutsidePublicSessionDom) {
  ExtensionTestMessageListener launched_listener("Launched", true);
  LoadAndLaunchPlatformApp("leave_fullscreen", &launched_listener);

  // We start by making sure the window is actually focused.
  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
      GetFirstAppWindow()->GetNativeWindow()));

  launched_listener.Reply("dom");

  // Because the DOM way to go fullscreen requires user gesture, we simulate a
  // key event to get the window entering in fullscreen mode. The reply will
  // make the window listen for the key event. The reply will be sent to the
  // renderer process before the keypress and should be received in that order.
  // When receiving the key event, the application will try to go fullscreen
  // using the Window API but there is no synchronous way to know if that
  // actually succeeded. Also, failure will not be notified. A failure case will
  // only be known with a timeout.
  {
    FullscreenChangeWaiter fs_changed(GetFirstAppWindow()->GetBaseWindow());

    WaitUntilKeyFocus();
    ASSERT_TRUE(SimulateKeyPress(ui::VKEY_A));

    fs_changed.Wait();
  }

  EXPECT_FALSE(window()->IsImmersiveModeEnabled());
  EXPECT_FALSE(window()->exclusive_access_bubble_);
}

IN_PROC_BROWSER_TEST_F(ChromeNativeAppWindowViewsAuraAshBrowserTest,
                       ImmersiveAndBubbleInsidePublicSessionWindow) {
  chromeos::ScopedTestPublicSessionLoginState state;
  ExtensionTestMessageListener launched_listener("Launched", true);
  LoadAndLaunchPlatformApp("leave_fullscreen", &launched_listener);

  // We start by making sure the window is actually focused.
  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
      GetFirstAppWindow()->GetNativeWindow()));

  // When receiving the reply, the application will try to go fullscreen using
  // the Window API but there is no synchronous way to know if that actually
  // succeeded. Also, failure will not be notified. A failure case will only be
  // known with a timeout.
  {
    FullscreenChangeWaiter fs_changed(GetFirstAppWindow()->GetBaseWindow());

    launched_listener.Reply("window");

    fs_changed.Wait();
  }

  EXPECT_TRUE(window()->IsImmersiveModeEnabled());
  EXPECT_TRUE(window()->exclusive_access_bubble_);
}

IN_PROC_BROWSER_TEST_F(ChromeNativeAppWindowViewsAuraAshBrowserTest,
                       ImmersiveAndBubbleInsidePublicSessionDom) {
  chromeos::ScopedTestPublicSessionLoginState state;
  ExtensionTestMessageListener launched_listener("Launched", true);
  LoadAndLaunchPlatformApp("leave_fullscreen", &launched_listener);

  // We start by making sure the window is actually focused.
  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
      GetFirstAppWindow()->GetNativeWindow()));

  launched_listener.Reply("dom");

  // Because the DOM way to go fullscreen requires user gesture, we simulate a
  // key event to get the window entering in fullscreen mode. The reply will
  // make the window listen for the key event. The reply will be sent to the
  // renderer process before the keypress and should be received in that order.
  // When receiving the key event, the application will try to go fullscreen
  // using the Window API but there is no synchronous way to know if that
  // actually succeeded. Also, failure will not be notified. A failure case will
  // only be known with a timeout.
  {
    FullscreenChangeWaiter fs_changed(GetFirstAppWindow()->GetBaseWindow());

    WaitUntilKeyFocus();
    ASSERT_TRUE(SimulateKeyPress(ui::VKEY_A));

    fs_changed.Wait();
  }

  EXPECT_TRUE(window()->IsImmersiveModeEnabled());
  EXPECT_TRUE(window()->exclusive_access_bubble_);
}

// Tests that the window opens on the correct display.
IN_PROC_BROWSER_TEST_F(ChromeNativeAppWindowViewsAuraAshBrowserTest,
                       OpenOnTheCorrectDisplay) {
  // Create two displays side by side.
  display::DisplayManager* display_manager =
      ash::ShellTestApi().display_manager();
  display::test::DisplayManagerTestApi(display_manager)
      .UpdateDisplay("800x800,800+0-800x800");
  ASSERT_EQ(2, display::Screen::GetScreen()->GetNumDisplays());

  const extensions::Extension* extension =
      LoadAndLaunchPlatformApp("launch", "Launched");
  // A window key is required to use the bounds saving AppWindowGeometryCache.
  extensions::AppWindow::CreateParams params;
  params.window_key = "test_key";
  extensions::AppWindow* app_window =
      CreateAppWindowFromParams(browser()->profile(), extension, params);
  // Current located on primary display.
  EXPECT_TRUE(gfx::Rect(800, 800).Contains(
      app_window->GetNativeWindow()->GetBoundsInScreen()));
  CloseAppWindow(app_window);

  // Set the display for new windows to be the secondary display.
  int64_t secondary_id = display::Screen::GetScreen()->GetAllDisplays()[1].id();
  display::Screen::GetScreen()->SetDisplayForNewWindows(secondary_id);

  // Create the same app. It should be located on the secondary display.
  app_window =
      CreateAppWindowFromParams(browser()->profile(), extension, params);
  EXPECT_TRUE(
      gfx::Rect(800, 0, 800, 800)
          .Contains(app_window->GetNativeWindow()->GetBoundsInScreen()));
  CloseAppWindow(app_window);

  // Update the secondary display to be shifted below and smaller than the
  // window bounds. Test that newly opened windows fit into the display bounds.
  display::test::DisplayManagerTestApi(display_manager)
      .UpdateDisplay("800x800,800+400-400x400");
  ASSERT_EQ(2, display::Screen::GetScreen()->GetNumDisplays());
  secondary_id = display::Screen::GetScreen()->GetAllDisplays()[1].id();
  display::Screen::GetScreen()->SetDisplayForNewWindows(secondary_id);
  app_window =
      CreateAppWindowFromParams(browser()->profile(), extension, params);
  // The display is physically lower, but the display manager puts all displays
  // in a horizontal line (i.e. three monitors vertically stacked will look
  // something like 800x800,800+0-800x800,1600+0-800x800).
  EXPECT_TRUE(
      gfx::Rect(800, 0, 400, 400)
          .Contains(app_window->GetNativeWindow()->GetBoundsInScreen()));
  EXPECT_EQ(secondary_id,
            display::Screen::GetScreen()
                ->GetDisplayNearestWindow(app_window->GetNativeWindow())
                .id());
  CloseAppWindow(app_window);

  // Tests that if there are no saved bounds, the window will be centered in the
  // work area. Shelf height is 48 so a display of size 800x848 will have a work
  // area of 800x800.
  display::test::DisplayManagerTestApi(display_manager)
      .UpdateDisplay("800x848,800+0-800x848");
  extensions::AppWindow::BoundsSpecification window_spec;
  window_spec.bounds = gfx::Rect(INT_MIN, INT_MIN, 400, 400);
  params.window_key = "";
  params.window_spec = window_spec;
  app_window =
      CreateAppWindowFromParams(browser()->profile(), extension, params);
  EXPECT_EQ(gfx::Rect(200, 200, 400, 400),
            app_window->GetNativeWindow()->GetBoundsInScreen());
  CloseAppWindow(app_window);

  // Tests that the window will be centered on the secondary display.
  ASSERT_EQ(2, display::Screen::GetScreen()->GetNumDisplays());
  secondary_id = display::Screen::GetScreen()->GetAllDisplays()[1].id();
  display::Screen::GetScreen()->SetDisplayForNewWindows(secondary_id);
  app_window =
      CreateAppWindowFromParams(browser()->profile(), extension, params);
  EXPECT_EQ(gfx::Rect(1000, 200, 400, 400),
            app_window->GetNativeWindow()->GetBoundsInScreen());
  CloseAppWindow(app_window);
}
