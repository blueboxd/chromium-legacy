// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_LOGGED_IN_USER_MIXIN_H_
#define CHROME_BROWSER_SUPERVISED_USER_LOGGED_IN_USER_MIXIN_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/login/test/embedded_test_server_mixin.h"
#include "chrome/browser/chromeos/login/test/fake_gaia_mixin.h"
#include "chrome/browser/chromeos/login/test/local_policy_test_server_mixin.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/test/user_policy_mixin.h"

namespace chromeos {

// Compound mixin class for child user browser tests.
// Supports logging in as regular or child accounts.
// Initiates other mixins required to log in users, sets up their user policies
// and gaia auth.
class LoggedInUserMixin {
 public:
  enum class LogInType { kRegular, kChild };

  LoggedInUserMixin(InProcessBrowserTestMixinHost* host,
                    LogInType type,
                    net::EmbeddedTestServer* embedded_test_server);
  ~LoggedInUserMixin();

  // Log in as regular or child account depending on the |type| argument passed
  // to the constructor.
  // * If |issue_any_scope_token|, FakeGaiaMixin will issue a special all-access
  // token associated with the test refresh token. Only matters for child login.
  void LogInUser(bool issue_any_scope_token = false);

  // By default, LoginManagerMixin will set up user session manager not to
  // launch browser as part of user session setup - use this to override that
  // behavior.
  void set_should_launch_browser(bool value);

 private:
  LoginManagerMixin::TestUserInfo user_;
  LoginManagerMixin login_manager_;

  LocalPolicyTestServerMixin policy_server_;
  UserPolicyMixin user_policy_;

  EmbeddedTestServerSetupMixin embedded_test_server_setup_;
  FakeGaiaMixin fake_gaia_;

  DISALLOW_COPY_AND_ASSIGN(LoggedInUserMixin);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_SUPERVISED_USER_LOGGED_IN_USER_MIXIN_H_
