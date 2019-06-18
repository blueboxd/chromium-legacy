// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen_ui/login_screen_extension_ui_handler.h"

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/extensions/login_screen_ui/login_screen_extension_ui_handler.h"
#include "chrome/browser/chromeos/policy/signin_profile_extensions_policy_test_base.h"
#include "components/version_info/version_info.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// The extension files can be found under the directory
// chrome/test/data/extensions/api_test/login_screen_ui/whitelisted_extension/
// This extension has been whitelisted to use the chrome.loginScreenUi API
// under chrome/common/extensions/api/_permission_features.json
const char kExtensionId[] = "oclffehlkdgibkainkilopaalpdobkan";
const char kExtensionUpdateManifestPath[] =
    "/extensions/api_test/login_screen_ui/update_manifest.xml";

const char kWaitingForTestName[] = "Waiting for test name";

const char kCanOpenWindow[] = "CanOpenWindow";
const char kCannotOpenMultipleWindows[] = "CannotOpenMultipleWindows";
const char kCanOpenAndCloseWindow[] = "CanOpenAndCloseWindow";
const char kCannotCloseNoWindow[] = "CannotCloseNoWindow";

}  // namespace

namespace chromeos {

class LoginScreenExtensionUiHandlerBrowsertest
    : public policy::SigninProfileExtensionsPolicyTestBase {
 public:
  LoginScreenExtensionUiHandlerBrowsertest()
      : SigninProfileExtensionsPolicyTestBase(version_info::Channel::DEV) {}

  ~LoginScreenExtensionUiHandlerBrowsertest() override = default;

  void SetUpExtensionAndRunTest(std::string testName) {
    extensions::ResultCatcher catcher;

    ExtensionTestMessageListener listener(kWaitingForTestName,
                                          /*will_reply=*/true);

    AddExtensionForForceInstallation(kExtensionId,
                                     kExtensionUpdateManifestPath);

    ASSERT_TRUE(listener.WaitUntilSatisfied());
    listener.Reply(testName);

    ASSERT_TRUE(catcher.GetNextResult());
  }

  bool HasOpenWindow() {
    LoginScreenExtensionUiHandler* ui_handler =
        LoginScreenExtensionUiHandler::Get(false);
    CHECK(ui_handler);
    return ui_handler->HasOpenWindow(kExtensionId);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginScreenExtensionUiHandlerBrowsertest);
};

IN_PROC_BROWSER_TEST_F(LoginScreenExtensionUiHandlerBrowsertest,
                       ExtensionCanOpenWindow) {
  SetUpExtensionAndRunTest(kCanOpenWindow);
  EXPECT_TRUE(HasOpenWindow());
}

IN_PROC_BROWSER_TEST_F(LoginScreenExtensionUiHandlerBrowsertest,
                       ExtensionCannotOpenMultipleWindows) {
  SetUpExtensionAndRunTest(kCannotOpenMultipleWindows);
  EXPECT_TRUE(HasOpenWindow());
}

IN_PROC_BROWSER_TEST_F(LoginScreenExtensionUiHandlerBrowsertest,
                       ExtensionCanOpenAndCloseWindow) {
  SetUpExtensionAndRunTest(kCanOpenAndCloseWindow);
  EXPECT_FALSE(HasOpenWindow());
}

IN_PROC_BROWSER_TEST_F(LoginScreenExtensionUiHandlerBrowsertest,
                       ExtensionCannotCloseNoWindow) {
  SetUpExtensionAndRunTest(kCannotCloseNoWindow);
  EXPECT_FALSE(HasOpenWindow());
}

}  // namespace chromeos
