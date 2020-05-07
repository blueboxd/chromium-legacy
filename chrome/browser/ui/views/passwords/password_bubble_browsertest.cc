// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"

#include <memory>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/passwords/manage_passwords_test.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/passwords/password_auto_sign_in_view.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/test/browser_test.h"

using base::StartsWith;

// Test params:
//  - bool : whether to enable account storage feature or not.
class PasswordBubbleBrowserTest
    : public SupportsTestDialog<ManagePasswordsTest>,
      public testing::WithParamInterface<bool> {
 public:
  PasswordBubbleBrowserTest() {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          password_manager::features::kEnablePasswordsAccountStorage);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          password_manager::features::kEnablePasswordsAccountStorage);
    }
  }

  ~PasswordBubbleBrowserTest() override = default;

  void ShowUi(const std::string& name) override {
    if (StartsWith(name, "PendingPasswordBubble",
                   base::CompareCase::SENSITIVE)) {
      SetupPendingPassword();
    } else if (StartsWith(name, "AutomaticPasswordBubble",
                          base::CompareCase::SENSITIVE)) {
      SetupAutomaticPassword();
    } else if (StartsWith(name, "ManagePasswordBubble",
                          base::CompareCase::SENSITIVE)) {
      SetupManagingPasswords();
      ExecuteManagePasswordsCommand();
    } else if (StartsWith(name, "AutoSignin", base::CompareCase::SENSITIVE)) {
      test_form()->origin = GURL("https://example.com");
      test_form()->display_name = base::ASCIIToUTF16("Peter");
      test_form()->username_value = base::ASCIIToUTF16("pet12@gmail.com");
      std::vector<std::unique_ptr<autofill::PasswordForm>> local_credentials;
      local_credentials.push_back(
          std::make_unique<autofill::PasswordForm>(*test_form()));

      PasswordAutoSignInView::set_auto_signin_toast_timeout(10);
      SetupAutoSignin(std::move(local_credentials));
    } else {
      ADD_FAILURE() << "Unknown dialog type";
      return;
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  DISALLOW_COPY_AND_ASSIGN(PasswordBubbleBrowserTest);
};

INSTANTIATE_TEST_SUITE_P(All, PasswordBubbleBrowserTest, ::testing::Bool());

IN_PROC_BROWSER_TEST_P(PasswordBubbleBrowserTest,
                       InvokeUi_PendingPasswordBubble) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PasswordBubbleBrowserTest,
                       InvokeUi_AutomaticPasswordBubble) {
  ShowAndVerifyUi();
}

// Disabled: ExecuteManagePasswordsCommand() spins a runloop which will be flaky
// in a browser test. See http://crbug.com/716681.
IN_PROC_BROWSER_TEST_P(PasswordBubbleBrowserTest,
                       DISABLED_InvokeUi_ManagePasswordBubble) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PasswordBubbleBrowserTest, InvokeUi_AutoSignin) {
  ShowAndVerifyUi();
}
