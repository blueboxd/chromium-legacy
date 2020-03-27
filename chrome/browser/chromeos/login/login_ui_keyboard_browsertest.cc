// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/login_screen_test_api.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/input_method/input_method_persistence.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/test_utils.h"

namespace chromeos {

namespace {

constexpr char kTestUser1[] = "test-user1@gmail.com";
constexpr char kTestUser1GaiaId[] = "1111111111";
constexpr char kTestUser2[] = "test-user2@gmail.com";
constexpr char kTestUser2GaiaId[] = "2222222222";
constexpr char kTestUser3[] = "test-user3@gmail.com";
constexpr char kTestUser3GaiaId[] = "3333333333";

void Append_en_US_InputMethods(std::vector<std::string>* out) {
  out->push_back("xkb:us::eng");
  out->push_back("xkb:us:intl:eng");
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  out->push_back("xkb:us:intl_pc:eng");
#endif
  out->push_back("xkb:us:altgr-intl:eng");
  out->push_back("xkb:us:dvorak:eng");
  out->push_back("xkb:us:dvp:eng");
  out->push_back("xkb:us:colemak:eng");
  out->push_back("xkb:us:workman:eng");
  out->push_back("xkb:us:workman-intl:eng");
  chromeos::input_method::InputMethodManager::Get()->MigrateInputMethods(out);
}

}  // anonymous namespace

class LoginUIKeyboardTest : public chromeos::LoginManagerTest {
 public:
  LoginUIKeyboardTest()
      : LoginManagerTest(false, false /* should_initialize_webui */) {
    set_force_webui_login(false);
    test_users_.push_back(
        AccountId::FromUserEmailGaiaId(kTestUser1, kTestUser1GaiaId));
    test_users_.push_back(
        AccountId::FromUserEmailGaiaId(kTestUser2, kTestUser2GaiaId));
  }
  ~LoginUIKeyboardTest() override {}

  void SetUpOnMainThread() override {
    user_input_methods.push_back("xkb:fr::fra");
    user_input_methods.push_back("xkb:de::ger");

    chromeos::input_method::InputMethodManager::Get()->MigrateInputMethods(
        &user_input_methods);

    LoginManagerTest::SetUpOnMainThread();
  }

  // Should be called from PRE_ test so that local_state is saved to disk, and
  // reloaded in the main test.
  void InitUserLastInputMethod() {
    PrefService* local_state = g_browser_process->local_state();

    input_method::SetUserLastInputMethodPreferenceForTesting(
        kTestUser1, user_input_methods[0], local_state);
    input_method::SetUserLastInputMethodPreferenceForTesting(
        kTestUser2, user_input_methods[1], local_state);
  }

 protected:
  std::vector<std::string> user_input_methods;
  std::vector<AccountId> test_users_;
};

IN_PROC_BROWSER_TEST_F(LoginUIKeyboardTest, PRE_CheckPODScreenDefault) {
  RegisterUser(test_users_[0]);
  RegisterUser(test_users_[1]);

  StartupUtils::MarkOobeCompleted();
}

// Check default IME initialization, when there is no IME configuration in
// local_state.
IN_PROC_BROWSER_TEST_F(LoginUIKeyboardTest, CheckPODScreenDefault) {
  EXPECT_EQ(2, ash::LoginScreenTestApi::GetUsersCount());
  EXPECT_EQ(test_users_[0], ash::LoginScreenTestApi::GetFocusedUser());

  std::vector<std::string> expected_input_methods;
  Append_en_US_InputMethods(&expected_input_methods);

  EXPECT_EQ(expected_input_methods, input_method::InputMethodManager::Get()
                                        ->GetActiveIMEState()
                                        ->GetActiveInputMethodIds());
}

IN_PROC_BROWSER_TEST_F(LoginUIKeyboardTest, PRE_CheckPODScreenWithUsers) {
  RegisterUser(test_users_[0]);
  RegisterUser(test_users_[1]);

  InitUserLastInputMethod();

  StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(LoginUIKeyboardTest, CheckPODScreenWithUsers) {
  EXPECT_EQ(2, ash::LoginScreenTestApi::GetUsersCount());
  EXPECT_EQ(test_users_[0], ash::LoginScreenTestApi::GetFocusedUser());

  EXPECT_EQ(user_input_methods[0], input_method::InputMethodManager::Get()
                                       ->GetActiveIMEState()
                                       ->GetCurrentInputMethod()
                                       .id());

  std::vector<std::string> expected_input_methods;
  Append_en_US_InputMethods(&expected_input_methods);
  // Active IM for the first user (active user POD).
  expected_input_methods.push_back(user_input_methods[0]);

  EXPECT_EQ(expected_input_methods, input_method::InputMethodManager::Get()
                                        ->GetActiveIMEState()
                                        ->GetActiveInputMethodIds());

  EXPECT_TRUE(ash::LoginScreenTestApi::FocusUser(test_users_[1]));

  EXPECT_EQ(user_input_methods[1], input_method::InputMethodManager::Get()
                                       ->GetActiveIMEState()
                                       ->GetCurrentInputMethod()
                                       .id());
}

class LoginUIKeyboardTestWithUsersAndOwner : public chromeos::LoginManagerTest {
 public:
  LoginUIKeyboardTestWithUsersAndOwner()
      : LoginManagerTest(false, false /* should_initialize_webui */) {
    set_force_webui_login(false);
  }
  ~LoginUIKeyboardTestWithUsersAndOwner() override {}

  void SetUpOnMainThread() override {
    user_input_methods.push_back("xkb:fr::fra");
    user_input_methods.push_back("xkb:de::ger");
    user_input_methods.push_back("xkb:pl::pol");

    chromeos::input_method::InputMethodManager::Get()->MigrateInputMethods(
        &user_input_methods);

    scoped_testing_cros_settings_.device_settings()->Set(
        kDeviceOwner, base::Value(kTestUser3));

    LoginManagerTest::SetUpOnMainThread();
  }

  // Should be called from PRE_ test so that local_state is saved to disk, and
  // reloaded in the main test.
  void InitUserLastInputMethod() {
    PrefService* local_state = g_browser_process->local_state();

    input_method::SetUserLastInputMethodPreferenceForTesting(
        kTestUser1, user_input_methods[0], local_state);
    input_method::SetUserLastInputMethodPreferenceForTesting(
        kTestUser2, user_input_methods[1], local_state);
    input_method::SetUserLastInputMethodPreferenceForTesting(
        kTestUser3, user_input_methods[2], local_state);

    local_state->SetString(language_prefs::kPreferredKeyboardLayout,
                           user_input_methods[2]);
  }

  void CheckGaiaKeyboard();

 protected:
  std::vector<std::string> user_input_methods;
  ScopedTestingCrosSettings scoped_testing_cros_settings_;
};

void LoginUIKeyboardTestWithUsersAndOwner::CheckGaiaKeyboard() {
  std::vector<std::string> expected_input_methods;
  // kPreferredKeyboardLayout is now set to last focused POD.
  expected_input_methods.push_back(user_input_methods[0]);
  // Locale default input methods (the first one also is hardware IM).
  Append_en_US_InputMethods(&expected_input_methods);

  EXPECT_EQ(expected_input_methods, input_method::InputMethodManager::Get()
                                        ->GetActiveIMEState()
                                        ->GetActiveInputMethodIds());
}

IN_PROC_BROWSER_TEST_F(LoginUIKeyboardTestWithUsersAndOwner,
                       PRE_CheckPODScreenKeyboard) {
  RegisterUser(AccountId::FromUserEmailGaiaId(kTestUser1, kTestUser1GaiaId));
  RegisterUser(AccountId::FromUserEmailGaiaId(kTestUser2, kTestUser2GaiaId));
  RegisterUser(AccountId::FromUserEmailGaiaId(kTestUser3, kTestUser3GaiaId));

  InitUserLastInputMethod();

  StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(LoginUIKeyboardTestWithUsersAndOwner,
                       CheckPODScreenKeyboard) {
  EXPECT_EQ(3, ash::LoginScreenTestApi::GetUsersCount());

  std::vector<std::string> expected_input_methods;
  // Owner input method.
  expected_input_methods.push_back(user_input_methods[2]);
  // Locale default input methods (the first one also is hardware IM).
  Append_en_US_InputMethods(&expected_input_methods);
  // Active IM for the first user (active user POD).
  expected_input_methods.push_back(user_input_methods[0]);

  EXPECT_EQ(expected_input_methods, input_method::InputMethodManager::Get()
                                        ->GetActiveIMEState()
                                        ->GetActiveInputMethodIds());

  // Switch to Gaia.
  ASSERT_TRUE(ash::LoginScreenTestApi::ClickAddUserButton());
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
  CheckGaiaKeyboard();

  const auto update_count = ash::LoginScreenTestApi::GetUiUpdateCount();
  // Switch back.
  test::ExecuteOobeJS("$('gaia-signin').cancel()");
  ash::LoginScreenTestApi::WaitForUiUpdate(update_count);
  EXPECT_FALSE(ash::LoginScreenTestApi::IsOobeDialogVisible());

  EXPECT_EQ(expected_input_methods, input_method::InputMethodManager::Get()
                                        ->GetActiveIMEState()
                                        ->GetActiveInputMethodIds());
}
}  // namespace chromeos
