// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/login_screen_test_api.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/reauth_stats.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/session/user_session_manager_test_api.h"
#include "chrome/browser/chromeos/login/signin_specifics.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/test/oobe_window_visibility_waiter.h"
#include "chrome/browser/chromeos/login/test/session_manager_state_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chromeos/login/auth/stub_authenticator.h"
#include "chromeos/login/auth/stub_authenticator_builder.h"
#include "chromeos/login/auth/user_context.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"

namespace chromeos {

namespace {

const char kUserEmail[] = "test-user@gmail.com";
const char kGaiaID[] = "111111";
const char kTokenHandle[] = "test_token_handle";

}  // namespace

class PasswordChangeTestBase : public LoginManagerTest {
 public:
  PasswordChangeTestBase() = default;
  ~PasswordChangeTestBase() override = default;

 protected:
  UserContext GetTestUserContext() {
    return login_mixin_.CreateDefaultUserContext(test_user_info_);
  }

  void OpenGaiaDialog(const AccountId& account_id) {
    EXPECT_FALSE(ash::LoginScreenTestApi::IsOobeDialogVisible());
    EXPECT_TRUE(ash::LoginScreenTestApi::IsForcedOnlineSignin(account_id));
    EXPECT_TRUE(ash::LoginScreenTestApi::FocusUser(account_id));
    OobeScreenWaiter(GaiaView::kScreenId).Wait();
    EXPECT_TRUE(ash::LoginScreenTestApi::IsOobeDialogVisible());
  }

  // Sets up UserSessionManager to use stub authenticator that reports a
  // password change, and attempts login.
  // Password changed OOBE dialog is expected to show up after calling this.
  void SetUpStubAuthentcatorAndAttemptLogin(const std::string& old_password) {
    EXPECT_TRUE(ash::LoginScreenTestApi::IsOobeDialogVisible());
    UserContext user_context = GetTestUserContext();

    auto authenticator_builder =
        std::make_unique<StubAuthenticatorBuilder>(user_context);
    authenticator_builder->SetUpPasswordChange(
        old_password,
        base::BindRepeating(
            &PasswordChangeTestBase::HandleDataRecoveryStatusChange,
            base::Unretained(this)));
    login_mixin_.AttemptLoginUsingAuthenticator(
        user_context, std::move(authenticator_builder));
  }

  void WaitForPasswordChangeScreen() {
    OobeScreenWaiter(OobeScreen::SCREEN_PASSWORD_CHANGED).Wait();
    OobeWindowVisibilityWaiter(true).Wait();

    EXPECT_FALSE(ash::LoginScreenTestApi::IsShutdownButtonShown());
    EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());
    EXPECT_FALSE(ash::LoginScreenTestApi::IsAddUserButtonShown());
  }

 protected:
  const AccountId test_account_id_ =
      AccountId::FromUserEmailGaiaId(kUserEmail, kGaiaID);
  const LoginManagerMixin::TestUserInfo test_user_info_{
      test_account_id_, user_manager::UserType::USER_TYPE_REGULAR,
      user_manager::User::OAuthTokenStatus::OAUTH2_TOKEN_STATUS_INVALID};
  LoginManagerMixin login_mixin_{&mixin_host_, {test_user_info_}};
  StubAuthenticator::DataRecoveryStatus data_recovery_status_ =
      StubAuthenticator::DataRecoveryStatus::kNone;

 private:
  void HandleDataRecoveryStatusChange(
      StubAuthenticator::DataRecoveryStatus status) {
    EXPECT_EQ(StubAuthenticator::DataRecoveryStatus::kNone,
              data_recovery_status_);
    data_recovery_status_ = status;
  }
};

using PasswordChangeTest = PasswordChangeTestBase;

IN_PROC_BROWSER_TEST_F(PasswordChangeTest, MigrateOldCryptohome) {
  OpenGaiaDialog(test_account_id_);

  base::HistogramTester histogram_tester;
  SetUpStubAuthentcatorAndAttemptLogin("old user password");
  WaitForPasswordChangeScreen();
  histogram_tester.ExpectBucketCount("Login.PasswordChanged.ReauthReason",
                                     ReauthReason::OTHER, 1);

  test::OobeJS()
      .CreateVisibilityWaiter(true,
                              {"gaia-password-changed", "oldPasswordCard"})
      ->Wait();

  // Fill out and submit the old password passed to the stub authenticator.
  test::OobeJS().TypeIntoPath("old user password",
                              {"gaia-password-changed", "oldPasswordInput"});
  test::OobeJS().ClickOnPath(
      {"gaia-password-changed", "oldPasswordInputForm", "button"});

  // User session should start, and whole OOBE screen is expected to be hidden,
  OobeWindowVisibilityWaiter(false).Wait();
  EXPECT_EQ(StubAuthenticator::DataRecoveryStatus::kRecovered,
            data_recovery_status_);

  login_mixin_.WaitForActiveSession();
}

IN_PROC_BROWSER_TEST_F(PasswordChangeTest, RetryOnWrongPassword) {
  OpenGaiaDialog(test_account_id_);
  SetUpStubAuthentcatorAndAttemptLogin("old user password");
  WaitForPasswordChangeScreen();
  test::OobeJS()
      .CreateVisibilityWaiter(true,
                              {"gaia-password-changed", "oldPasswordCard"})
      ->Wait();

  // Fill out and submit the old password passed to the stub authenticator.
  test::OobeJS().TypeIntoPath("incorrect old user password",
                              {"gaia-password-changed", "oldPasswordInput"});
  test::OobeJS().ClickOnPath(
      {"gaia-password-changed", "oldPasswordInputForm", "button"});

  // Expect the UI to report failure.
  test::OobeJS()
      .CreateWaiter(test::GetOobeElementPath(
                        {"gaia-password-changed", "oldPasswordInput"}) +
                    ".invalid")
      ->Wait();
  test::OobeJS().ExpectEnabledPath(
      {"gaia-password-changed", "oldPasswordCard"});

  EXPECT_EQ(StubAuthenticator::DataRecoveryStatus::kRecoveryFailed,
            data_recovery_status_);

  data_recovery_status_ = StubAuthenticator::DataRecoveryStatus::kNone;

  // Submit the correct password.
  test::OobeJS().TypeIntoPath("old user password",
                              {"gaia-password-changed", "oldPasswordInput"});
  test::OobeJS().ClickOnPath(
      {"gaia-password-changed", "oldPasswordInputForm", "button"});

  // User session should start, and whole OOBE screen is expected to be hidden,
  OobeWindowVisibilityWaiter(false).Wait();
  EXPECT_EQ(StubAuthenticator::DataRecoveryStatus::kRecovered,
            data_recovery_status_);

  login_mixin_.WaitForActiveSession();
}

IN_PROC_BROWSER_TEST_F(PasswordChangeTest, SkipDataRecovery) {
  OpenGaiaDialog(test_account_id_);
  SetUpStubAuthentcatorAndAttemptLogin("old user password");
  WaitForPasswordChangeScreen();
  test::OobeJS()
      .CreateVisibilityWaiter(true,
                              {"gaia-password-changed", "oldPasswordCard"})
      ->Wait();

  // Click forgot password link.
  test::OobeJS().ClickOnPath({"gaia-password-changed", "forgot-password-link"});

  test::OobeJS()
      .CreateDisplayedWaiter(false,
                             {"gaia-password-changed", "oldPasswordCard"})
      ->Wait();

  test::OobeJS().ExpectVisiblePath({"gaia-password-changed", "try-again-link"});
  test::OobeJS().ExpectVisiblePath(
      {"gaia-password-changed", "proceedAnywayBtn"});

  // Click "Proceed anyway".
  test::OobeJS().ClickOnPath({"gaia-password-changed", "proceedAnywayBtn"});

  // User session should start, and whole OOBE screen is expected to be hidden,
  OobeWindowVisibilityWaiter(false).Wait();
  EXPECT_EQ(StubAuthenticator::DataRecoveryStatus::kResynced,
            data_recovery_status_);

  login_mixin_.WaitForActiveSession();
}

IN_PROC_BROWSER_TEST_F(PasswordChangeTest, TryAgainAfterForgetLinkClick) {
  OpenGaiaDialog(test_account_id_);
  SetUpStubAuthentcatorAndAttemptLogin("old user password");
  WaitForPasswordChangeScreen();
  test::OobeJS()
      .CreateDisplayedWaiter(true, {"gaia-password-changed", "oldPasswordCard"})
      ->Wait();

  // Click forgot password link.
  test::OobeJS().ClickOnPath({"gaia-password-changed", "forgot-password-link"});

  test::OobeJS()
      .CreateDisplayedWaiter(false,
                             {"gaia-password-changed", "oldPasswordCard"})
      ->Wait();

  test::OobeJS().ExpectVisiblePath({"gaia-password-changed", "try-again-link"});
  test::OobeJS().ExpectVisiblePath(
      {"gaia-password-changed", "proceedAnywayBtn"});

  // Go back to old password input by clicking Try Again.
  test::OobeJS().ClickOnPath({"gaia-password-changed", "try-again-link"});

  test::OobeJS()
      .CreateDisplayedWaiter(true, {"gaia-password-changed", "oldPasswordCard"})
      ->Wait();

  // Enter and submit the correct password.
  test::OobeJS().TypeIntoPath("old user password",
                              {"gaia-password-changed", "oldPasswordInput"});
  test::OobeJS().ClickOnPath(
      {"gaia-password-changed", "oldPasswordInputForm", "button"});

  // User session should start, and whole OOBE screen is expected to be hidden,
  OobeWindowVisibilityWaiter(false).Wait();
  EXPECT_EQ(StubAuthenticator::DataRecoveryStatus::kRecovered,
            data_recovery_status_);

  login_mixin_.WaitForActiveSession();
}

IN_PROC_BROWSER_TEST_F(PasswordChangeTest, ClosePasswordChangedDialog) {
  OpenGaiaDialog(test_account_id_);
  SetUpStubAuthentcatorAndAttemptLogin("old user password");
  WaitForPasswordChangeScreen();
  test::OobeJS()
      .CreateVisibilityWaiter(true,
                              {"gaia-password-changed", "oldPasswordCard"})
      ->Wait();

  test::OobeJS().TypeIntoPath("old user password",
                              {"gaia-password-changed", "oldPasswordInput"});
  // Click the close button.
  test::OobeJS().ClickOnPath(
      {"gaia-password-changed", "navigation", "closeButton"});

  OobeWindowVisibilityWaiter(false).Wait();
  EXPECT_EQ(StubAuthenticator::DataRecoveryStatus::kNone,
            data_recovery_status_);

  ExistingUserController::current_controller()->Login(GetTestUserContext(),
                                                      SigninSpecifics());
  OobeWindowVisibilityWaiter(true).Wait();
  OobeScreenWaiter(OobeScreen::SCREEN_PASSWORD_CHANGED).Wait();
}

class PasswordChangeTokenCheck : public PasswordChangeTestBase {
 public:
  PasswordChangeTokenCheck() {
    login_mixin_.AppendRegularUsers(1);
    user_with_invalid_token_ = login_mixin_.users().back().account_id;
  }

 protected:
  // PasswordChangeTestBase:
  void SetUpInProcessBrowserTestFixture() override {
    PasswordChangeTestBase::SetUpInProcessBrowserTestFixture();
    TokenHandleUtil::SetInvalidTokenForTesting(kTokenHandle);
  }
  void TearDownInProcessBrowserTestFixture() override {
    TokenHandleUtil::SetInvalidTokenForTesting(nullptr);
    PasswordChangeTestBase::TearDownInProcessBrowserTestFixture();
  }

  AccountId user_with_invalid_token_;
};

IN_PROC_BROWSER_TEST_F(PasswordChangeTokenCheck, LoginScreenPasswordChange) {
  TokenHandleUtil::StoreTokenHandle(user_with_invalid_token_, kTokenHandle);
  // Focus triggers token check.
  ash::LoginScreenTestApi::FocusUser(user_with_invalid_token_);

  OpenGaiaDialog(user_with_invalid_token_);
  base::HistogramTester histogram_tester;
  SetUpStubAuthentcatorAndAttemptLogin("old user password");
  WaitForPasswordChangeScreen();
  histogram_tester.ExpectBucketCount("Login.PasswordChanged.ReauthReason",
                                     ReauthReason::INVALID_TOKEN_HANDLE, 1);
}

IN_PROC_BROWSER_TEST_F(PasswordChangeTokenCheck, LoginScreenNoPasswordChange) {
  TokenHandleUtil::StoreTokenHandle(user_with_invalid_token_, kTokenHandle);
  // Focus triggers token check.
  ash::LoginScreenTestApi::FocusUser(user_with_invalid_token_);

  OpenGaiaDialog(user_with_invalid_token_);
  base::HistogramTester histogram_tester;
  // Does not trigger password change screen.
  login_mixin_.LoginWithDefaultContext(login_mixin_.users().back());
  login_mixin_.WaitForActiveSession();
  histogram_tester.ExpectBucketCount("Login.PasswordNotChanged.ReauthReason",
                                     ReauthReason::INVALID_TOKEN_HANDLE, 1);
}

// Helper class to create NotificationDisplayServiceTester before notification
// in the session shown.
class ProfileWaiter : public ProfileManagerObserver {
 public:
  ProfileWaiter() { g_browser_process->profile_manager()->AddObserver(this); }
  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override {
    g_browser_process->profile_manager()->RemoveObserver(this);
    display_service_ =
        std::make_unique<NotificationDisplayServiceTester>(profile);
    run_loop_.Quit();
  }

  std::unique_ptr<NotificationDisplayServiceTester> Wait() {
    run_loop_.Run();
    return std::move(display_service_);
  }

 private:
  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
  base::RunLoop run_loop_;
};

// Tests token handle check on the session start.
IN_PROC_BROWSER_TEST_F(PasswordChangeTokenCheck, PRE_Session) {
  // Focus triggers token check. User does not have stored token, so online
  // login should not be forced.
  ash::LoginScreenTestApi::FocusUser(user_with_invalid_token_);
  ASSERT_FALSE(
      ash::LoginScreenTestApi::IsForcedOnlineSignin(user_with_invalid_token_));

  // Store invalid token to triger notification in the session.
  TokenHandleUtil::StoreTokenHandle(user_with_invalid_token_, kTokenHandle);

  ProfileWaiter waiter;
  login_mixin_.LoginWithDefaultContext(login_mixin_.users().back());
  // We need to replace notification service very early to intercept reauth
  // notification.
  auto display_service_tester = waiter.Wait();

  login_mixin_.WaitForActiveSession();

  std::vector<message_center::Notification> notifications =
      display_service_tester->GetDisplayedNotificationsForType(
          NotificationHandler::Type::TRANSIENT);
  ASSERT_EQ(notifications.size(), 1u);

  // Click on notification should trigger Chrome restart.
  content::WindowedNotificationObserver exit_waiter(
      chrome::NOTIFICATION_APP_TERMINATING,
      content::NotificationService::AllSources());
  display_service_tester->SimulateClick(NotificationHandler::Type::TRANSIENT,
                                        notifications[0].id(), base::nullopt,
                                        base::nullopt);
  exit_waiter.Wait();
}

IN_PROC_BROWSER_TEST_F(PasswordChangeTokenCheck, Session) {
  ASSERT_TRUE(
      ash::LoginScreenTestApi::IsForcedOnlineSignin(user_with_invalid_token_));
  OpenGaiaDialog(user_with_invalid_token_);

  base::HistogramTester histogram_tester;
  SetUpStubAuthentcatorAndAttemptLogin("old user password");
  WaitForPasswordChangeScreen();
  histogram_tester.ExpectBucketCount("Login.PasswordChanged.ReauthReason",
                                     ReauthReason::INVALID_TOKEN_HANDLE, 1);
}

}  // namespace chromeos
