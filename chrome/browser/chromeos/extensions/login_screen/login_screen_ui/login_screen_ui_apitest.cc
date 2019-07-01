// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "chrome/browser/chromeos/extensions/login_screen/login_screen_apitest_base.h"
#include "chrome/browser/chromeos/extensions/login_screen/login_screen_ui/login_screen_extension_ui_handler.h"
#include "chrome/browser/chromeos/policy/signin_profile_extensions_policy_test_base.h"
#include "components/version_info/version_info.h"

namespace {

const char kCanOpenWindow[] = "LoginScreenUiCanOpenWindow";
const char kCannotOpenMultipleWindows[] =
    "LoginScreenUiCannotOpenMultipleWindows";
const char kCanOpenAndCloseWindow[] = "LoginScreenUiCanOpenAndCloseWindow";
const char kCannotCloseNoWindow[] = "LoginScreenUiCannotCloseNoWindow";

}  // namespace

namespace chromeos {

class LoginScreenUiApitest : public LoginScreenApitestBase {
 public:
  LoginScreenUiApitest() : LoginScreenApitestBase(version_info::Channel::DEV) {}

  ~LoginScreenUiApitest() override = default;

  bool HasOpenWindow() {
    LoginScreenExtensionUiHandler* ui_handler =
        LoginScreenExtensionUiHandler::Get(false);
    CHECK(ui_handler);
    return ui_handler->HasOpenWindow(extension_id_);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginScreenUiApitest);
};

IN_PROC_BROWSER_TEST_F(LoginScreenUiApitest, ExtensionCanOpenWindow) {
  SetUpExtensionAndRunTest(kCanOpenWindow);
  EXPECT_TRUE(HasOpenWindow());
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

}  // namespace chromeos
