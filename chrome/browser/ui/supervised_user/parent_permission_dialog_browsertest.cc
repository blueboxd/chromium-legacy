// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/supervised_user/parent_permission_dialog.h"

#include <memory>
#include <vector>

#include "base/run_loop.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/logged_in_user_mixin.h"
#include "chrome/browser/supervised_user/supervised_user_features.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/extension_builder.h"
#include "google_apis/gaia/fake_gaia.h"

// End to end test of ParentPermissionDialog that exercises the dialog's
// internal logic the orchestrates the parental permission process.
class ParentPermissionDialogBrowserTest
    : public MixinBasedInProcessBrowserTest {
 public:
  ParentPermissionDialogBrowserTest() = default;

  ParentPermissionDialogBrowserTest(const ParentPermissionDialogBrowserTest&) =
      delete;
  ParentPermissionDialogBrowserTest& operator=(
      const ParentPermissionDialogBrowserTest&) = delete;

  void OnParentPermissionDialogDone(base::OnceClosure quit_closure,
                                    ParentPermissionDialog::Result result) {
    result_ = result;
    std::move(quit_closure).Run();
  }

  void InitializeFamilyData() {
    // Set up the child user's custodians (AKA parents).
    ASSERT_TRUE(browser());
    PrefService* prefs = browser()->profile()->GetPrefs();
    prefs->SetString(prefs::kSupervisedUserCustodianEmail,
                     "test_parent_0@google.com");
    prefs->SetString(prefs::kSupervisedUserCustodianObfuscatedGaiaId,
                     "239029320");

    prefs->SetString(prefs::kSupervisedUserSecondCustodianEmail,
                     "test_parent_1@google.com");
    prefs->SetString(prefs::kSupervisedUserSecondCustodianObfuscatedGaiaId,
                     "85948533");

    // Set up the identity test environment, which provides fake
    // OAuth refresh tokens.
    identity_test_env_ = std::make_unique<signin::IdentityTestEnvironment>();
    identity_test_env_->MakeAccountAvailable(
        chromeos::FakeGaiaMixin::kFakeUserEmail);
    identity_test_env_->SetPrimaryAccount(
        chromeos::FakeGaiaMixin::kFakeUserEmail);
    identity_test_env_->SetRefreshTokenForPrimaryAccount();
    identity_test_env_->SetAutomaticIssueOfAccessTokens(true);
    ParentPermissionDialog::SetFakeIdentityManagerForTesting(
        identity_test_env_->identity_manager());
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    logged_in_user_mixin_.LogInUser(true /* issue_any_scope_token */);
    InitializeFamilyData();
    SupervisedUserService* service =
        SupervisedUserServiceFactory::GetForProfile(browser()->profile());
    service->SetSupervisedUserExtensionsMayRequestPermissionsPrefForTesting(
        true);
  }

  void SetNextReAuthStatus(
      const GaiaAuthConsumer::ReAuthProofTokenStatus next_status) {
    logged_in_user_mixin_.GetFakeGaiaMixin()->fake_gaia()->SetNextReAuthStatus(
        next_status);
  }

  void ShowPrompt() {
    base::RunLoop run_loop;

    parent_permission_dialog_ = std::make_unique<ParentPermissionDialog>(
        browser()->profile(),
        base::BindOnce(
            &ParentPermissionDialogBrowserTest::OnParentPermissionDialogDone,
            base::Unretained(this), run_loop.QuitClosure()));

    parent_permission_dialog_->set_reprompt_after_incorrect_credential(false);
    SkBitmap icon =
        *gfx::Image(extensions::util::GetDefaultExtensionIcon()).ToSkBitmap();
    parent_permission_dialog_->ShowPrompt(
        browser()->tab_strip_model()->GetActiveWebContents(),
        base::UTF8ToUTF16("Test Prompt Message"), icon);
    run_loop.Run();
  }

  void ShowPromptForExtension(
      scoped_refptr<const extensions::Extension> extension) {
    base::RunLoop run_loop;

    parent_permission_dialog_ = std::make_unique<ParentPermissionDialog>(
        browser()->profile(),
        base::BindOnce(
            &ParentPermissionDialogBrowserTest::OnParentPermissionDialogDone,
            base::Unretained(this), run_loop.QuitClosure()));

    parent_permission_dialog_->set_reprompt_after_incorrect_credential(false);
    SkBitmap icon =
        *gfx::Image(extensions::util::GetDefaultExtensionIcon()).ToSkBitmap();
    parent_permission_dialog_->ShowPromptForExtensionInstallation(
        browser()->tab_strip_model()->GetActiveWebContents(), extension.get(),
        icon);
    run_loop.Run();
  }

  void CheckResult(ParentPermissionDialog::Result expected) {
    EXPECT_EQ(result_, expected);
  }

  void CheckInvalidCredentialWasReceived() {
    EXPECT_TRUE(parent_permission_dialog_->CredentialWasInvalid());
  }

 private:
  ParentPermissionDialog::Result result_;
  std::unique_ptr<ParentPermissionDialog> parent_permission_dialog_;

  chromeos::LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, chromeos::LoggedInUserMixin::LogInType::kChild,
      embedded_test_server(), this};

  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_env_;
};

IN_PROC_BROWSER_TEST_F(ParentPermissionDialogBrowserTest, PermissionReceived) {
  SetNextReAuthStatus(GaiaAuthConsumer::ReAuthProofTokenStatus::kSuccess);
  SetAutoConfirmParentPermissionDialogForTest(
      internal::ParentPermissionDialogViewResult::Status::kAccepted);
  ShowPrompt();
  CheckResult(ParentPermissionDialog::Result::kParentPermissionReceived);
}

IN_PROC_BROWSER_TEST_F(ParentPermissionDialogBrowserTest,
                       PermissionFailedInvalidPassword) {
  SetNextReAuthStatus(GaiaAuthConsumer::ReAuthProofTokenStatus::kInvalidGrant);
  SetAutoConfirmParentPermissionDialogForTest(
      internal::ParentPermissionDialogViewResult::Status::kAccepted);
  ShowPrompt();
  CheckInvalidCredentialWasReceived();
  CheckResult(ParentPermissionDialog::Result::kParentPermissionFailed);
}

IN_PROC_BROWSER_TEST_F(ParentPermissionDialogBrowserTest,
                       PermissionDialogCanceled) {
  SetAutoConfirmParentPermissionDialogForTest(
      internal::ParentPermissionDialogViewResult::Status::kCanceled);
  ShowPrompt();
  CheckResult(ParentPermissionDialog::Result::kParentPermissionCanceled);
}

IN_PROC_BROWSER_TEST_F(ParentPermissionDialogBrowserTest,
                       PermissionReceivedForExtension) {
  SetNextReAuthStatus(GaiaAuthConsumer::ReAuthProofTokenStatus::kSuccess);
  SetAutoConfirmParentPermissionDialogForTest(
      internal::ParentPermissionDialogViewResult::Status::kAccepted);
  ShowPromptForExtension(
      extensions::ExtensionBuilder("test extension").Build());
  CheckResult(ParentPermissionDialog::Result::kParentPermissionReceived);
}

IN_PROC_BROWSER_TEST_F(ParentPermissionDialogBrowserTest,
                       PermissionFailedInvalidPasswordForExtension) {
  SetNextReAuthStatus(GaiaAuthConsumer::ReAuthProofTokenStatus::kInvalidGrant);
  SetAutoConfirmParentPermissionDialogForTest(
      internal::ParentPermissionDialogViewResult::Status::kAccepted);
  ShowPromptForExtension(
      extensions::ExtensionBuilder("test extension").Build());
  CheckInvalidCredentialWasReceived();
  CheckResult(ParentPermissionDialog::Result::kParentPermissionFailed);
}

IN_PROC_BROWSER_TEST_F(ParentPermissionDialogBrowserTest,
                       PermissionDialogCanceledForExtension) {
  SetAutoConfirmParentPermissionDialogForTest(
      internal::ParentPermissionDialogViewResult::Status::kCanceled);

  ShowPromptForExtension(
      extensions::ExtensionBuilder("test extension").Build());
  CheckResult(ParentPermissionDialog::Result::kParentPermissionCanceled);
}
