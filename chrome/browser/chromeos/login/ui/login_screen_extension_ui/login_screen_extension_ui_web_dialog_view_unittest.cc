// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/login_screen_extension_ui/login_screen_extension_ui_web_dialog_view.h"

#include <memory>

#include "base/bind_helpers.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/login/ui/login_screen_extension_ui/login_screen_extension_ui_create_options.h"
#include "chrome/browser/chromeos/login/ui/login_screen_extension_ui/login_screen_extension_ui_dialog_delegate.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chromeos {

class LoginScreenExtensionUiWebDialogViewUnittest : public testing::Test {
 public:
  LoginScreenExtensionUiWebDialogViewUnittest() = default;
  ~LoginScreenExtensionUiWebDialogViewUnittest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginScreenExtensionUiWebDialogViewUnittest);
};

TEST_F(LoginScreenExtensionUiWebDialogViewUnittest, ShouldShowCloseButton) {
  LoginScreenExtensionUiCreateOptions create_options(
      "extension_name", GURL(),
      /*can_be_closed_by_user=*/true, base::DoNothing());

  std::unique_ptr<LoginScreenExtensionUiDialogDelegate> dialog_delegate =
      std::make_unique<LoginScreenExtensionUiDialogDelegate>(&create_options);

  std::unique_ptr<LoginScreenExtensionUiWebDialogView> dialog_view =
      std::make_unique<LoginScreenExtensionUiWebDialogView>(
          &profile, dialog_delegate.get(),
          std::make_unique<ChromeWebContentsHandler>());

  ASSERT_TRUE(dialog_view->ShouldShowCloseButton());
}

TEST_F(LoginScreenExtensionUiWebDialogViewUnittest, ShouldNotShowCloseButton) {
  LoginScreenExtensionUiCreateOptions create_options(
      "extension_name", GURL(),
      /*can_be_closed_by_user=*/false, base::DoNothing());

  std::unique_ptr<LoginScreenExtensionUiDialogDelegate> dialog_delegate =
      std::make_unique<LoginScreenExtensionUiDialogDelegate>(&create_options);

  std::unique_ptr<LoginScreenExtensionUiWebDialogView> dialog_view =
      std::make_unique<LoginScreenExtensionUiWebDialogView>(
          &profile, dialog_delegate.get(),
          std::make_unique<ChromeWebContentsHandler>());

  ASSERT_FALSE(dialog_view->ShouldShowCloseButton());
}

}  // namespace chromeos
