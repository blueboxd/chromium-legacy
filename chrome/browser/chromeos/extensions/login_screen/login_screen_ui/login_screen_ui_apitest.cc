// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login_screen_ui/login_screen_extension_ui_handler.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "components/session_manager/core/session_manager.h"
#include "components/version_info/version_info.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/test/test_api.h"
#include "extensions/common/features/feature_channel.h"

namespace extensions {

// Class for testing the loginScreenUi API using different browser channels.
class LoginScreenUiApiPerChannelTest
    : public ExtensionApiTest,
      public testing::WithParamInterface<version_info::Channel> {
 public:
  LoginScreenUiApiPerChannelTest()
      : channel_(GetParam()), scoped_current_channel_(GetParam()) {}

 protected:
  const version_info::Channel channel_;
  const extensions::ScopedCurrentChannel scoped_current_channel_;

  DISALLOW_COPY_AND_ASSIGN(LoginScreenUiApiPerChannelTest);
};

// Api not available on all browser channels for non-whitelisted extension.
IN_PROC_BROWSER_TEST_P(LoginScreenUiApiPerChannelTest,
                       NotAvailableForNotWhitelistedExtension) {
  EXPECT_TRUE(RunExtensionSubtest(
      "login_screen_ui/not_whitelisted_extension.crx", "test.html",
      kFlagLoadForLoginScreen | kFlagIgnoreManifestWarnings))
      << message_;
}

// API available to whitelisted extension on unknown, canary and dev browser
// channels, but not on beta and stable channels.
IN_PROC_BROWSER_TEST_P(LoginScreenUiApiPerChannelTest,
                       AvailableForWhitelistedExtension) {
  const std::string whitelisted_extension =
      "login_screen_ui/whitelisted_extension.crx";

  switch (channel_) {
    case version_info::Channel::UNKNOWN:
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV:
      EXPECT_TRUE(RunExtensionSubtest(whitelisted_extension,
                                      "test.html?apiAvailable",
                                      kFlagLoadForLoginScreen))
          << message_;
      break;
    case version_info::Channel::BETA:
    case version_info::Channel::STABLE:
      EXPECT_TRUE(RunExtensionSubtest(
          whitelisted_extension, "test.html?apiNotAvailable",
          kFlagLoadForLoginScreen | kFlagIgnoreManifestWarnings))
          << message_;
      break;
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         LoginScreenUiApiPerChannelTest,
                         testing::Values(version_info::Channel::UNKNOWN,
                                         version_info::Channel::CANARY,
                                         version_info::Channel::DEV,
                                         version_info::Channel::BETA,
                                         version_info::Channel::STABLE));

}  // namespace extensions
