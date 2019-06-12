// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_MOCK_LOGIN_SCREEN_CLIENT_H_
#define ASH_LOGIN_MOCK_LOGIN_SCREEN_CLIENT_H_

#include "ash/public/cpp/login_screen_client.h"
#include "components/password_manager/core/browser/hash_password_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockLoginScreenClient : public LoginScreenClient {
 public:
  MockLoginScreenClient();
  ~MockLoginScreenClient() override;

  MOCK_METHOD4(AuthenticateUserWithPasswordOrPin_,
               void(const AccountId& account_id,
                    const std::string& password,
                    bool authenticated_by_pin,
                    base::OnceCallback<void(bool)>& callback));
  MOCK_METHOD2(AuthenticateUserWithExternalBinary_,
               void(const AccountId& account_id,
                    base::OnceCallback<void(bool)>& callback));
  MOCK_METHOD1(EnrollUserWithExternalBinary_,
               void(base::OnceCallback<void(bool)>& callback));
  MOCK_METHOD2(ValidateParentAccessCode_,
               bool(const AccountId& account_id,
                    const std::string& access_code));

  // Set the result that should be passed to |callback| in
  // |AuthenticateUserWithPasswordOrPin| or
  // |AuthenticateUserWithExternalBinary|.
  void set_authenticate_user_callback_result(bool value) {
    authenticate_user_callback_result_ = value;
  }

  // Sets the result that should be passed to |callback| in
  // |ValidateParentAccessCode|.
  void set_validate_parent_access_code_result(bool value) {
    validate_parent_access_code_result_ = value;
  }

  // If set to non-null, when |AuthenticateUser| is called the callback will be
  // stored in |storage| instead of being executed.
  void set_authenticate_user_with_password_or_pin_callback_storage(
      base::OnceCallback<void(bool)>* storage) {
    authenticate_user_with_password_or_pin_callback_storage_ = storage;
  }
  void set_authenticate_user_with_external_binary_storage(
      base::OnceCallback<void(bool)>* storage) {
    authenticate_user_with_external_binary_callback_storage_ = storage;
  }
  void set_enroll_user_with_external_binary_storage(
      base::OnceCallback<void(bool)>* storage) {
    enroll_user_with_external_binary_callback_storage_ = storage;
  }

  // LoginScreenClient:
  void AuthenticateUserWithPasswordOrPin(
      const AccountId& account_id,
      const std::string& password,
      bool authenticated_by_pin,
      base::OnceCallback<void(bool)> callback) override;
  void AuthenticateUserWithExternalBinary(
      const AccountId& account_id,
      base::OnceCallback<void(bool)> callback) override;
  void EnrollUserWithExternalBinary(
      base::OnceCallback<void(bool)> callback) override;
  bool ValidateParentAccessCode(const AccountId& account_id,
                                const std::string& code) override;
  MOCK_METHOD1(AuthenticateUserWithEasyUnlock,
               void(const AccountId& account_id));
  MOCK_METHOD1(HardlockPod, void(const AccountId& account_id));
  MOCK_METHOD1(OnFocusPod, void(const AccountId& account_id));
  MOCK_METHOD0(OnNoPodFocused, void());
  MOCK_METHOD1(LoadWallpaper, void(const AccountId& account_id));
  MOCK_METHOD0(SignOutUser, void());
  MOCK_METHOD0(CancelAddUser, void());
  MOCK_METHOD0(LoginAsGuest, void());
  MOCK_METHOD1(OnMaxIncorrectPasswordAttempted,
               void(const AccountId& account_id));
  MOCK_METHOD1(FocusLockScreenApps, void(bool reverse));
  MOCK_METHOD2(ShowGaiaSignin,
               void(bool can_close, const AccountId& prefilled_account));
  MOCK_METHOD0(OnRemoveUserWarningShown, void());
  MOCK_METHOD1(RemoveUser, void(const AccountId& account_id));
  MOCK_METHOD3(LaunchPublicSession,
               void(const AccountId& account_id,
                    const std::string& locale,
                    const std::string& input_method));
  MOCK_METHOD2(RequestPublicSessionKeyboardLayouts,
               void(const AccountId& account_id, const std::string& locale));
  MOCK_METHOD0(ShowFeedback, void());
  MOCK_METHOD0(ShowResetScreen, void());
  MOCK_METHOD0(ShowAccountAccessHelpApp, void());
  MOCK_METHOD0(ShowLockScreenNotificationSettings, void());
  MOCK_METHOD0(FocusOobeDialog, void());
  MOCK_METHOD1(OnFocusLeavingSystemTray, void(bool reverse));
  MOCK_METHOD0(OnUserActivity, void());

 private:
  bool authenticate_user_callback_result_ = true;
  bool validate_parent_access_code_result_ = true;
  base::OnceCallback<void(bool)>*
      authenticate_user_with_password_or_pin_callback_storage_ = nullptr;
  base::OnceCallback<void(bool)>*
      authenticate_user_with_external_binary_callback_storage_ = nullptr;
  base::OnceCallback<void(bool)>*
      enroll_user_with_external_binary_callback_storage_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(MockLoginScreenClient);
};

}  // namespace ash

#endif  // ASH_LOGIN_MOCK_LOGIN_SCREEN_CLIENT_H_
