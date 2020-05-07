// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/supervised_user/parent_permission_dialog.h"

#include <memory>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/logged_in_user_mixin.h"
#include "chrome/browser/supervised_user/supervised_user_features.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/parent_permission_dialog_view.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/extension_builder.h"
#include "google_apis/gaia/fake_gaia.h"

// End to end test of ParentPermissionDialog that exercises the dialog's
// internal logic the orchestrates the parental permission process.
class ParentPermissionDialogBrowserTest
    : public SupportsTestDialog<MixinBasedInProcessBrowserTest>,
      public TestParentPermissionDialogViewObserver {
 public:
  // The next dialog action to take.
  enum class NextDialogAction {
    kCancel,
    kAccept,
  };

  ParentPermissionDialogBrowserTest()
      : TestParentPermissionDialogViewObserver(this) {}

  ParentPermissionDialogBrowserTest(const ParentPermissionDialogBrowserTest&) =
      delete;
  ParentPermissionDialogBrowserTest& operator=(
      const ParentPermissionDialogBrowserTest&) = delete;

  void OnParentPermissionDialogDone(ParentPermissionDialog::Result result) {
    result_ = result;
    std::move(on_dialog_done_closure_).Run();
  }

  // TestBrowserUi
  void ShowUi(const std::string& name) override {
    SkBitmap icon =
        *gfx::Image(extensions::util::GetDefaultExtensionIcon()).ToSkBitmap();
    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    // Set up a RunLoop with a quit closure to block until
    // the dialog is shown, which is what this method is supposed
    // to ensure.
    base::RunLoop run_loop;
    dialog_shown_closure_ = run_loop.QuitClosure();

    // These use base::DoNothing because we aren't interested in the dialog's
    // results. Unlike the other non-TestBrowserUi tests, this test doesn't
    // block, because that interferes with the widget accounting done by
    // TestBrowserUi.
    if (name == "default") {
      parent_permission_dialog_ =
          ParentPermissionDialog::CreateParentPermissionDialog(
              browser()->profile(), contents,
              contents->GetTopLevelNativeWindow(),
              gfx::ImageSkia::CreateFrom1xBitmap(icon),
              base::UTF8ToUTF16("Test prompt message"), base::DoNothing());
    } else if (name == "extension") {
      parent_permission_dialog_ =
          ParentPermissionDialog::CreateParentPermissionDialogForExtension(
              browser()->profile(), contents,
              contents->GetTopLevelNativeWindow(),
              gfx::ImageSkia::CreateFrom1xBitmap(icon), test_extension_.get(),
              base::DoNothing());
    }
    parent_permission_dialog_->ShowDialog();
    run_loop.Run();
  }

  // TestParentPermissionDialogViewObserver
  void OnTestParentPermissionDialogViewCreated(
      ParentPermissionDialogView* view) override {
    if (dialog_shown_closure_)
      std::move(dialog_shown_closure_).Run();

    view_ = view;
    view_->SetIdentityManagerForTesting(identity_test_env_->identity_manager());
    view_->SetRepromptAfterIncorrectCredential(false);

    if (next_dialog_action_) {
      switch (next_dialog_action_.value()) {
        case NextDialogAction::kCancel:
          view->CancelDialog();
          break;
        case NextDialogAction::kAccept:
          view->AcceptDialog();
          break;
      }
    }
  }

  void InitializeFamilyData() {
    // Set up the child user's custodians (AKA parents).
    ASSERT_TRUE(browser());
    supervised_user_test_util::AddCustodians(browser()->profile());

    // Set up the identity test environment, which provides fake
    // OAuth refresh tokens.
    identity_test_env_ = std::make_unique<signin::IdentityTestEnvironment>();
    identity_test_env_->MakeAccountAvailable(
        chromeos::FakeGaiaMixin::kFakeUserEmail);
    identity_test_env_->SetPrimaryAccount(
        chromeos::FakeGaiaMixin::kFakeUserEmail);
    identity_test_env_->SetRefreshTokenForPrimaryAccount();
    identity_test_env_->SetAutomaticIssueOfAccessTokens(true);
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    test_extension_ = extensions::ExtensionBuilder("test extension").Build();
    logged_in_user_mixin_.LogInUser(true /* issue_any_scope_token */);
    InitializeFamilyData();
    SupervisedUserService* service =
        SupervisedUserServiceFactory::GetForProfile(browser()->profile());
    service->SetSupervisedUserExtensionsMayRequestPermissionsPrefForTesting(
        true);
  }

  void set_next_reauth_status(
      const GaiaAuthConsumer::ReAuthProofTokenStatus next_status) {
    logged_in_user_mixin_.GetFakeGaiaMixin()->fake_gaia()->SetNextReAuthStatus(
        next_status);
  }

  void set_next_dialog_action(NextDialogAction action) {
    next_dialog_action_ = action;
  }

  // This method will block until the next dialog completing action takes place,
  // so that the result can be checked.
  void ShowPrompt() {
    base::RunLoop run_loop;
    on_dialog_done_closure_ = run_loop.QuitClosure();
    ParentPermissionDialog::DoneCallback callback = base::BindOnce(
        &ParentPermissionDialogBrowserTest::OnParentPermissionDialogDone,
        base::Unretained(this));

    SkBitmap icon =
        *gfx::Image(extensions::util::GetDefaultExtensionIcon()).ToSkBitmap();
    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    parent_permission_dialog_ =
        ParentPermissionDialog::CreateParentPermissionDialog(
            browser()->profile(), contents, contents->GetTopLevelNativeWindow(),
            gfx::ImageSkia::CreateFrom1xBitmap(icon),
            base::UTF8ToUTF16("Test prompt message"), std::move(callback));
    parent_permission_dialog_->ShowDialog();
    run_loop.Run();
  }

  // This method will block until the next dialog action takes place, so that
  // the result can be checked.
  void ShowPromptForExtension() {
    base::RunLoop run_loop;
    on_dialog_done_closure_ = run_loop.QuitClosure();

    ParentPermissionDialog::DoneCallback callback = base::BindOnce(
        &ParentPermissionDialogBrowserTest::OnParentPermissionDialogDone,
        base::Unretained(this));

    SkBitmap icon =
        *gfx::Image(extensions::util::GetDefaultExtensionIcon()).ToSkBitmap();

    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    parent_permission_dialog_ =
        ParentPermissionDialog::CreateParentPermissionDialogForExtension(
            browser()->profile(), contents, contents->GetTopLevelNativeWindow(),
            gfx::ImageSkia::CreateFrom1xBitmap(icon), test_extension_.get(),
            std::move(callback));
    parent_permission_dialog_->ShowDialog();
    run_loop.Run();
  }

  void CheckResult(ParentPermissionDialog::Result expected) {
    EXPECT_EQ(result_, expected);
  }

  void CheckInvalidCredentialWasReceived() {
    EXPECT_TRUE(view_->invalid_credential_received());
  }

 private:
  ParentPermissionDialogView* view_ = nullptr;
  std::unique_ptr<ParentPermissionDialog> parent_permission_dialog_;

  ParentPermissionDialog::Result result_;

  chromeos::LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, chromeos::LoggedInUserMixin::LogInType::kChild,
      embedded_test_server(), this};

  // Closure that is triggered once the dialog is shown.
  base::OnceClosure dialog_shown_closure_;

  // Closure that is triggered once the dialog completes.
  base::OnceClosure on_dialog_done_closure_;

  scoped_refptr<const extensions::Extension> test_extension_ = nullptr;

  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_env_;
  base::Optional<NextDialogAction> next_dialog_action_;
};

// Tests that a plain dialog widget is shown using the TestBrowserUi
// infrastructure.
IN_PROC_BROWSER_TEST_F(ParentPermissionDialogBrowserTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

// Tests that the extension-parameterized dialog widget is shown using the
// TestBrowserUi infrastructure.
IN_PROC_BROWSER_TEST_F(ParentPermissionDialogBrowserTest, InvokeUi_extension) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ParentPermissionDialogBrowserTest, PermissionReceived) {
  set_next_reauth_status(GaiaAuthConsumer::ReAuthProofTokenStatus::kSuccess);
  set_next_dialog_action(
      ParentPermissionDialogBrowserTest::NextDialogAction::kAccept);
  ShowPrompt();
  CheckResult(ParentPermissionDialog::Result::kParentPermissionReceived);
}

IN_PROC_BROWSER_TEST_F(ParentPermissionDialogBrowserTest,
                       PermissionFailedInvalidPassword) {
  set_next_reauth_status(
      GaiaAuthConsumer::ReAuthProofTokenStatus::kInvalidGrant);
  set_next_dialog_action(
      ParentPermissionDialogBrowserTest::NextDialogAction::kAccept);
  ShowPrompt();
  CheckInvalidCredentialWasReceived();
  CheckResult(ParentPermissionDialog::Result::kParentPermissionFailed);
}

IN_PROC_BROWSER_TEST_F(ParentPermissionDialogBrowserTest,
                       PermissionDialogCanceled) {
  set_next_dialog_action(
      ParentPermissionDialogBrowserTest::NextDialogAction::kCancel);
  ShowPrompt();
  CheckResult(ParentPermissionDialog::Result::kParentPermissionCanceled);
}

IN_PROC_BROWSER_TEST_F(ParentPermissionDialogBrowserTest,
                       PermissionReceivedForExtension) {
  set_next_reauth_status(GaiaAuthConsumer::ReAuthProofTokenStatus::kSuccess);
  set_next_dialog_action(
      ParentPermissionDialogBrowserTest::NextDialogAction::kAccept);
  ShowPrompt();
  CheckResult(ParentPermissionDialog::Result::kParentPermissionReceived);
}

IN_PROC_BROWSER_TEST_F(ParentPermissionDialogBrowserTest,
                       PermissionFailedInvalidPasswordForExtension) {
  set_next_reauth_status(
      GaiaAuthConsumer::ReAuthProofTokenStatus::kInvalidGrant);
  set_next_dialog_action(
      ParentPermissionDialogBrowserTest::NextDialogAction::kAccept);
  ShowPromptForExtension();
  CheckInvalidCredentialWasReceived();
  CheckResult(ParentPermissionDialog::Result::kParentPermissionFailed);
}

IN_PROC_BROWSER_TEST_F(ParentPermissionDialogBrowserTest,
                       PermissionDialogCanceledForExtension) {
  set_next_dialog_action(
      ParentPermissionDialogBrowserTest::NextDialogAction::kCancel);

  ShowPromptForExtension();
  CheckResult(ParentPermissionDialog::Result::kParentPermissionCanceled);
}
