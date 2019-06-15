// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/test/app_window_waiter.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/test/fake_gaia_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/ui/ash/kiosk_next_shell_client.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_features.h"

namespace chromeos {
namespace {

class KioskNextShellClientTest : public OobeBaseTest {
 public:
  KioskNextShellClientTest() : OobeBaseTest() {
    feature_list_.InitAndEnableFeature(ash::features::kKioskNextShell);
  }

  void Login(const std::string& username) {
    WaitForSigninScreen();

    content::WindowedNotificationObserver session_started_observer(
        chrome::NOTIFICATION_SESSION_STARTED,
        content::NotificationService::AllSources());

    LoginDisplayHost::default_host()
        ->GetOobeUI()
        ->GetView<GaiaScreenHandler>()
        ->ShowSigninScreenForTest(username, "password", "[]");

    // Wait for the session to start after submitting the credentials. This
    // will wait until all the background requests are done.
    session_started_observer.Wait();
  }

  void LoginAndEnableKioskNextShellPref() {
    Login("username");

    // Take some time to finish login and register user prefs.
    base::RunLoop().RunUntilIdle();

    // Update the now registered Kiosk Next Shell pref.
    ProfileHelper::Get()
        ->GetProfileByUser(user_manager::UserManager::Get()->GetActiveUser())
        ->GetPrefs()
        ->SetBoolean(ash::prefs::kKioskNextShellEnabled, true);
  }

 private:
  FakeGaiaMixin fake_gaia_{&mixin_host_, embedded_test_server()};

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(KioskNextShellClientTest, PRE_KioskNextShellLaunch) {
  LoginAndEnableKioskNextShellPref();
}

// Checks that the Kiosk Next Home window is launched on sign-in when the
// feature is enabled and its pref allows it.
IN_PROC_BROWSER_TEST_F(KioskNextShellClientTest, KioskNextShellLaunch) {
  // Enable all component extensions.
  extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();

  Login("username");

  // Wait for the app to launch.
  apps::AppWindowWaiter waiter(
      extensions::AppWindowRegistry::Get(ProfileHelper::Get()->GetProfileByUser(
          user_manager::UserManager::Get()->GetActiveUser())),
      extension_misc::kKioskNextHomeAppId);
  EXPECT_TRUE(waiter.WaitForShownWithTimeout(TestTimeouts::action_timeout()));
}

IN_PROC_BROWSER_TEST_F(KioskNextShellClientTest, PRE_BrowserNotLaunched) {
  LoginAndEnableKioskNextShellPref();
}

// Ensures that browser is not launched when feature is enabled and prefs allow.
IN_PROC_BROWSER_TEST_F(KioskNextShellClientTest, BrowserNotLaunched) {
  extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();
  Login("username");
  Profile* profile = ProfileHelper::Get()->GetProfileByUser(
      user_manager::UserManager::Get()->GetActiveUser());
  EXPECT_EQ(0u, chrome::GetBrowserCount(profile));
}

// Checks that the Kiosk Next Home window does not launch in sign-in when
// its pref is disabled.
//
// See https://crbug.com/971256
#if defined(OS_LINUX)
#define MAYBE_KioskNextShellNotLaunched DISABLED_KioskNextShellNotLaunched
#else
#define MAYBE_KioskNextShellNotLaunched KioskNextShellNotLaunched
#endif
IN_PROC_BROWSER_TEST_F(KioskNextShellClientTest,
                       MAYBE_KioskNextShellNotLaunched) {
  // Enable all component extensions.
  extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();

  Login("username");

  // Wait for the app to launch before verifying that it didn't.
  apps::AppWindowWaiter waiter(
      extensions::AppWindowRegistry::Get(ProfileHelper::Get()->GetProfileByUser(
          user_manager::UserManager::Get()->GetActiveUser())),
      extension_misc::kKioskNextHomeAppId);
  EXPECT_FALSE(waiter.WaitForShownWithTimeout(TestTimeouts::action_timeout()));
}

// Variant of KioskNextShellClientTest that disables Mash in order to test the
// Ash container of the Chrome app window.
// TODO(crbug.com/945704): Once we can identify the app window's container with
// SingleProcessMash enabled, remove this test class and add the container check
// to the KioskNextShellLaunch test.
class KioskNextShellClientMashDisabledTest : public KioskNextShellClientTest {
 public:
  KioskNextShellClientMashDisabledTest() : KioskNextShellClientTest() {
    feature_list_.InitAndDisableFeature(features::kSingleProcessMash);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(KioskNextShellClientMashDisabledTest,
                       PRE_KioskNextShellLaunch) {
  LoginAndEnableKioskNextShellPref();
}

// Checks that the Kiosk Next Home window is launched on sign-in when the
// feature is enabled and its pref allows it.
IN_PROC_BROWSER_TEST_F(KioskNextShellClientMashDisabledTest,
                       KioskNextShellLaunch) {
  // Enable all component extensions.
  extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();

  Login("username");

  // Wait for the app to launch.
  apps::AppWindowWaiter waiter(
      extensions::AppWindowRegistry::Get(ProfileHelper::Get()->GetProfileByUser(
          user_manager::UserManager::Get()->GetActiveUser())),
      extension_misc::kKioskNextHomeAppId);
  extensions::AppWindow* app_window =
      waiter.WaitForShownWithTimeout(TestTimeouts::action_timeout());
  ASSERT_TRUE(app_window);

  // Verify the window is in the Home Screen container.
  EXPECT_EQ(app_window->GetNativeWindow()->parent(),
            app_window->GetNativeWindow()->GetRootWindow()->GetChildById(
                ash::kShellWindowId_HomeScreenContainer));
}

}  // namespace
}  // namespace chromeos
