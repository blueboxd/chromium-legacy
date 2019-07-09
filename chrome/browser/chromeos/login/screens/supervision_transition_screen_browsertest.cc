// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "ash/public/cpp/login_screen_test_api.h"
#include "base/run_loop.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/arc/arc_service_launcher.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/login/mixin_based_in_process_browser_test.h"
#include "chrome/browser/chromeos/login/test/embedded_test_server_mixin.h"
#include "chrome/browser/chromeos/login/test/fake_gaia_mixin.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/local_policy_test_server_mixin.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/test/user_policy_mixin.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/supervision_transition_screen_handler.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/login/auth/stub_authenticator_builder.h"
#include "chromeos/login/auth/user_context.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/session/arc_session_runner.h"
#include "components/arc/session/arc_supervision_transition.h"
#include "components/arc/test/fake_arc_session.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_type.h"
#include "net/dns/mock_host_resolver.h"

namespace chromeos {

// Param returns the original user type.
class SupervisionTransitionScreenTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<user_manager::UserType> {
 public:
  SupervisionTransitionScreenTest() = default;
  ~SupervisionTransitionScreenTest() override = default;

  // MixinBasedInProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kArcAvailability,
                                    "officially-supported");
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    ASSERT_TRUE(user_policy_.RequestPolicyUpdate());

    arc::ArcServiceLauncher::Get()->ResetForTesting();
    arc::ArcSessionManager::Get()->SetArcSessionRunnerForTesting(
        std::make_unique<arc::ArcSessionRunner>(
            base::BindRepeating(arc::FakeArcSession::Create)));

    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  }

  // The tests simulate user type changes between regular and child user.
  // This returns the intended user type after transition. GetParam() returns
  // the initial user type.
  user_manager::UserType GetTargetUserType() const {
    return GetParam() == user_manager::USER_TYPE_REGULAR
               ? user_manager::USER_TYPE_CHILD
               : user_manager::USER_TYPE_REGULAR;
  }

  void LogIn(const LoginManagerMixin::TestUserInfo& user) {
    UserContext user_context =
        LoginManagerMixin::CreateDefaultUserContext(user);
    if (user.user_type == user_manager::USER_TYPE_CHILD) {
      fake_gaia_.SetupFakeGaiaForChildUser(
          user.account_id.GetUserEmail(), user.account_id.GetGaiaId(),
          FakeGaiaMixin::kFakeRefreshToken, false /*issue_any_scope_token*/);
    } else {
      fake_gaia_.SetupFakeGaiaForLogin(user.account_id.GetUserEmail(),
                                       user.account_id.GetGaiaId(),
                                       FakeGaiaMixin::kFakeRefreshToken);
    }
    user_context.SetRefreshToken(FakeGaiaMixin::kFakeRefreshToken);
    login_manager_.AttemptLoginUsingAuthenticator(
        user_context, std::make_unique<StubAuthenticatorBuilder>(user_context));
  }

  LoginManagerMixin::TestUserInfo user_{
      AccountId::FromUserEmailGaiaId("user@gmail.com", "user"), GetParam()};
  LoginManagerMixin login_manager_{&mixin_host_, {user_}};

  LocalPolicyTestServerMixin policy_server_{&mixin_host_};
  UserPolicyMixin user_policy_{&mixin_host_, user_.account_id, &policy_server_};

  EmbeddedTestServerSetupMixin embedded_test_server_setup_{
      &mixin_host_, embedded_test_server()};
  FakeGaiaMixin fake_gaia_{&mixin_host_, embedded_test_server()};
};

IN_PROC_BROWSER_TEST_P(SupervisionTransitionScreenTest,
                       PRE_SuccessfulTransition) {
  LogIn(user_);
  login_manager_.WaitForActiveSession();

  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  profile->GetPrefs()->SetBoolean(arc::prefs::kArcSignedIn, true);
  arc::SetArcPlayStoreEnabledForProfile(profile, true);
}

IN_PROC_BROWSER_TEST_P(SupervisionTransitionScreenTest, SuccessfulTransition) {
  LoginManagerMixin::TestUserInfo transitioned_user(user_.account_id,
                                                    GetTargetUserType());
  LogIn(transitioned_user);

  OobeScreenWaiter(SupervisionTransitionScreenView::kScreenId).Wait();

  test::OobeJS().ExpectVisiblePath(
      {"supervision-transition-md", "supervisionTransitionDialog"});
  test::OobeJS().ExpectHiddenPath(
      {"supervision-transition-md", "supervisionTransitionErrorDialog"});

  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsAddUserButtonShown());

  ProfileManager::GetPrimaryUserProfile()->GetPrefs()->SetInteger(
      arc::prefs::kArcSupervisionTransition,
      static_cast<int>(arc::ArcSupervisionTransition::NO_TRANSITION));

  EXPECT_FALSE(ProfileManager::GetPrimaryUserProfile()->GetPrefs()->GetBoolean(
      arc::prefs::kArcDataRemoveRequested));

  login_manager_.WaitForActiveSession();
}

IN_PROC_BROWSER_TEST_P(SupervisionTransitionScreenTest, PRE_TransitionTimeout) {
  LogIn(user_);
  login_manager_.WaitForActiveSession();

  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  profile->GetPrefs()->SetBoolean(arc::prefs::kArcSignedIn, true);
  arc::SetArcPlayStoreEnabledForProfile(profile, true);
}

IN_PROC_BROWSER_TEST_P(SupervisionTransitionScreenTest, TransitionTimeout) {
  LoginManagerMixin::TestUserInfo transitioned_user(user_.account_id,
                                                    GetTargetUserType());
  LogIn(transitioned_user);

  OobeScreenWaiter(SupervisionTransitionScreenView::kScreenId).Wait();

  test::OobeJS().ExpectVisiblePath(
      {"supervision-transition-md", "supervisionTransitionDialog"});
  test::OobeJS().ExpectHiddenPath(
      {"supervision-transition-md", "supervisionTransitionErrorDialog"});

  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsAddUserButtonShown());

  base::OneShotTimer* timer =
      LoginDisplayHost::default_host()
          ->GetOobeUI()
          ->GetHandler<SupervisionTransitionScreenHandler>()
          ->GetTimerForTesting();
  ASSERT_TRUE(timer->IsRunning());
  timer->FireNow();

  EXPECT_TRUE(ProfileManager::GetPrimaryUserProfile()->GetPrefs()->GetBoolean(
      arc::prefs::kArcDataRemoveRequested));

  test::OobeJS()
      .CreateVisibilityWaiter(true, {"supervision-transition-md",
                                     "supervisionTransitionErrorDialog"})
      ->Wait();
  test::OobeJS().ExpectHiddenPath(
      {"supervision-transition-md", "supervisionTransitionDialog"});

  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsAddUserButtonShown());

  test::OobeJS().TapOnPath({"supervision-transition-md", "accept-button"});

  login_manager_.WaitForActiveSession();
}

IN_PROC_BROWSER_TEST_P(SupervisionTransitionScreenTest,
                       PRE_SkipTransitionIfArcNeverStarted) {
  LogIn(user_);
  login_manager_.WaitForActiveSession();
}

IN_PROC_BROWSER_TEST_P(SupervisionTransitionScreenTest,
                       SkipTransitionIfArcNeverStarted) {
  LoginManagerMixin::TestUserInfo transitioned_user(user_.account_id,
                                                    GetTargetUserType());

  // Login should go through without being interrupted.
  LogIn(transitioned_user);
  login_manager_.WaitForActiveSession();
}

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         SupervisionTransitionScreenTest,
                         testing::Values(user_manager::USER_TYPE_REGULAR,
                                         user_manager::USER_TYPE_CHILD));

}  // namespace chromeos
