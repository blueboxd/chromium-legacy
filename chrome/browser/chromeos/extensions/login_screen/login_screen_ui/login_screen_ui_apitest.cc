// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "chrome/browser/chromeos/extensions/login_screen/login_screen_apitest_base.h"
#include "chrome/browser/chromeos/extensions/login_screen/login_screen_ui/ui_handler.h"
#include "chrome/browser/chromeos/login/ui/login_screen_extension_ui/dialog_delegate.h"
#include "chrome/browser/chromeos/login/ui/login_screen_extension_ui/window.h"
#include "chrome/browser/chromeos/policy/signin_profile_extensions_policy_test_base.h"
#include "components/version_info/version_info.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace {

constexpr char kCanOpenWindow[] = "LoginScreenUiCanOpenWindow";
constexpr char kCannotOpenMultipleWindows[] =
    "LoginScreenUiCannotOpenMultipleWindows";
constexpr char kCanOpenAndCloseWindow[] = "LoginScreenUiCanOpenAndCloseWindow";
constexpr char kCannotCloseNoWindow[] = "LoginScreenUiCannotCloseNoWindow";
constexpr char kUserCanCloseWindow[] = "LoginScreenUiUserCanCloseWindow";
constexpr char kUserCannotCloseWindow[] = "LoginScreenUiUserCannotCloseWindow";

}  // namespace

namespace chromeos {

namespace login_screen_extension_ui {

class LoginScreenUiApitest : public LoginScreenApitestBase {
 public:
  LoginScreenUiApitest() : LoginScreenApitestBase(version_info::Channel::DEV) {}

  ~LoginScreenUiApitest() override = default;

  bool HasOpenWindow() const {
    return UiHandler::Get(false)->HasOpenWindow(extension_id_);
  }

  bool CanCloseDialog() const {
    return UiHandler::Get(false)
        ->GetWindowForTesting(extension_id_)
        ->GetDialogDelegateForTesting()
        ->CanCloseDialog();
  }

  bool ShouldShowCloseButton() const {
    return UiHandler::Get(false)
        ->GetWindowForTesting(extension_id_)
        ->GetDialogWidgetForTesting()
        ->widget_delegate()
        ->ShouldShowCloseButton();
  }

  bool IsMovementDisabled() const {
    return UiHandler::Get(false)
        ->GetWindowForTesting(extension_id_)
        ->GetDialogWidgetForTesting()
        ->movement_disabled();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginScreenUiApitest);
};

IN_PROC_BROWSER_TEST_F(LoginScreenUiApitest, ExtensionCanOpenWindow) {
  SetUpExtensionAndRunTest(kCanOpenWindow);
  ASSERT_TRUE(HasOpenWindow());
  // userCanClose defaults to false
  EXPECT_TRUE(IsMovementDisabled());
  EXPECT_FALSE(CanCloseDialog());
  EXPECT_FALSE(ShouldShowCloseButton());
}

IN_PROC_BROWSER_TEST_F(LoginScreenUiApitest,
                       ExtensionCannotOpenMultipleWindows) {
  SetUpExtensionAndRunTest(kCannotOpenMultipleWindows);
  EXPECT_TRUE(HasOpenWindow());
}

IN_PROC_BROWSER_TEST_F(LoginScreenUiApitest, ExtensionCanOpenAndCloseWindow) {
  SetUpExtensionAndRunTest(kCanOpenAndCloseWindow);
  EXPECT_FALSE(HasOpenWindow());
}

IN_PROC_BROWSER_TEST_F(LoginScreenUiApitest, ExtensionCannotCloseNoWindow) {
  SetUpExtensionAndRunTest(kCannotCloseNoWindow);
  EXPECT_FALSE(HasOpenWindow());
}

IN_PROC_BROWSER_TEST_F(LoginScreenUiApitest, UserCanCloseWindow) {
  SetUpExtensionAndRunTest(kUserCanCloseWindow);
  ASSERT_TRUE(HasOpenWindow());
  EXPECT_TRUE(IsMovementDisabled());
  EXPECT_TRUE(CanCloseDialog());
  EXPECT_TRUE(ShouldShowCloseButton());
}

IN_PROC_BROWSER_TEST_F(LoginScreenUiApitest, UserCannotCloseWindow) {
  SetUpExtensionAndRunTest(kUserCannotCloseWindow);
  ASSERT_TRUE(HasOpenWindow());
  EXPECT_TRUE(IsMovementDisabled());
  EXPECT_FALSE(CanCloseDialog());
  EXPECT_FALSE(ShouldShowCloseButton());
}

}  // namespace login_screen_extension_ui

}  // namespace chromeos
