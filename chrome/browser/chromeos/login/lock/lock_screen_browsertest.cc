// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/login_screen_test_api.h"
#include "chrome/browser/chromeos/login/lock/screen_locker_tester.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "components/user_manager/user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/chromeos/input_method_manager.h"

namespace chromeos {
namespace {

class LockScreenTest : public LoginManagerTest {
 public:
  LockScreenTest() { login_manager_.AppendRegularUsers(2); }

  void SetUpOnMainThread() override {
    LoginManagerTest::SetUpOnMainThread();
    user_input_methods_.push_back("xkb:fr::fra");
    user_input_methods_.push_back("xkb:de::ger");

    chromeos::input_method::InputMethodManager::Get()->MigrateInputMethods(
        &user_input_methods_);
  }

  ~LockScreenTest() override = default;

 protected:
  std::vector<std::string> user_input_methods_;
  LoginManagerMixin login_manager_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(LockScreenTest, CheckIMESwitches) {
  const auto& users = login_manager_.users();
  LoginUser(users[0].account_id);
  scoped_refptr<input_method::InputMethodManager::State> ime_states[2] = {
      nullptr, nullptr};
  input_method::InputMethodManager* input_manager =
      input_method::InputMethodManager::Get();
  ime_states[0] = input_manager->GetActiveIMEState();
  ASSERT_TRUE(ime_states[0]->EnableInputMethod(user_input_methods_[0]));
  ime_states[0]->ChangeInputMethod(user_input_methods_[0], false);
  EXPECT_EQ(ime_states[0]->GetCurrentInputMethod().id(),
            user_input_methods_[0]);

  UserAddingScreen::Get()->Start();
  AddUser(users[1].account_id);
  EXPECT_EQ(users[1].account_id,
            user_manager::UserManager::Get()->GetActiveUser()->GetAccountId());
  ime_states[1] = input_manager->GetActiveIMEState();
  ASSERT_TRUE(ime_states[1]->EnableInputMethod(user_input_methods_[1]));
  ime_states[1]->ChangeInputMethod(user_input_methods_[1], false);
  EXPECT_EQ(ime_states[1]->GetCurrentInputMethod().id(),
            user_input_methods_[1]);

  ASSERT_NE(ime_states[0], ime_states[1]);

  ScreenLockerTester locker_tester;
  locker_tester.Lock();
  EXPECT_EQ(2, ash::LoginScreenTestApi::GetUsersCount());
  // IME state should be lock screen specific.
  EXPECT_NE(ime_states[0], input_manager->GetActiveIMEState());
  EXPECT_NE(ime_states[1], input_manager->GetActiveIMEState());

  EXPECT_EQ(users[0].account_id, ash::LoginScreenTestApi::GetFocusedUser());
  EXPECT_EQ(input_manager->GetActiveIMEState()->GetCurrentInputMethod().id(),
            user_input_methods_[0]);
  locker_tester.UnlockWithPassword(users[0].account_id, "password");
  locker_tester.WaitForUnlock();
  EXPECT_EQ(users[0].account_id,
            user_manager::UserManager::Get()->GetActiveUser()->GetAccountId());
  EXPECT_EQ(ime_states[0], input_manager->GetActiveIMEState());
  EXPECT_EQ(ime_states[0]->GetCurrentInputMethod().id(),
            user_input_methods_[0]);

  locker_tester.Lock();
  EXPECT_EQ(2, ash::LoginScreenTestApi::GetUsersCount());
  // IME state should be lock screen specific.
  EXPECT_NE(ime_states[0], input_manager->GetActiveIMEState());
  EXPECT_NE(ime_states[1], input_manager->GetActiveIMEState());

  EXPECT_EQ(users[0].account_id, ash::LoginScreenTestApi::GetFocusedUser());
  EXPECT_EQ(input_manager->GetActiveIMEState()->GetCurrentInputMethod().id(),
            user_input_methods_[0]);
  EXPECT_TRUE(ash::LoginScreenTestApi::FocusUser(users[1].account_id));
  EXPECT_EQ(input_manager->GetActiveIMEState()->GetCurrentInputMethod().id(),
            user_input_methods_[1]);
  locker_tester.UnlockWithPassword(users[1].account_id, "password");
  EXPECT_EQ(users[1].account_id,
            user_manager::UserManager::Get()->GetActiveUser()->GetAccountId());
  EXPECT_EQ(ime_states[1], input_manager->GetActiveIMEState());
  EXPECT_EQ(ime_states[1]->GetCurrentInputMethod().id(),
            user_input_methods_[1]);
}

}  // namespace
}  // namespace chromeos
