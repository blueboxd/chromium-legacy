// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/logged_in_user_mixin.h"

#include "chromeos/login/auth/user_context.h"

namespace chromeos {

namespace {

constexpr char kTestUserName[] = "user@gmail.com";
constexpr char kTestUserGaiaId[] = "user";

user_manager::UserType ConvertUserType(LoggedInUserMixin::LogInType type) {
  switch (type) {
    case LoggedInUserMixin::LogInType::kChild:
      return user_manager::USER_TYPE_CHILD;
    case LoggedInUserMixin::LogInType::kRegular:
      return user_manager::USER_TYPE_REGULAR;
  }
}

}  // namespace

LoggedInUserMixin::LoggedInUserMixin(
    InProcessBrowserTestMixinHost* mixin_host,
    LogInType type,
    net::EmbeddedTestServer* embedded_test_server)
    : user_(AccountId::FromUserEmailGaiaId(kTestUserName, kTestUserGaiaId),
            ConvertUserType(type)),
      login_manager_(mixin_host, {user_}),
      policy_server_(mixin_host),
      user_policy_(mixin_host, user_.account_id, &policy_server_),
      embedded_test_server_setup_(mixin_host, embedded_test_server),
      fake_gaia_(mixin_host, embedded_test_server) {}

LoggedInUserMixin::~LoggedInUserMixin() = default;

void LoggedInUserMixin::LogInUser(bool issue_any_scope_token) {
  UserContext user_context = LoginManagerMixin::CreateDefaultUserContext(user_);
  if (user_.user_type == user_manager::USER_TYPE_CHILD) {
    fake_gaia_.SetupFakeGaiaForChildUser(
        user_.account_id.GetUserEmail(), user_.account_id.GetGaiaId(),
        FakeGaiaMixin::kFakeRefreshToken, issue_any_scope_token);
  } else {
    fake_gaia_.SetupFakeGaiaForLogin(user_.account_id.GetUserEmail(),
                                     user_.account_id.GetGaiaId(),
                                     FakeGaiaMixin::kFakeRefreshToken);
  }
  user_context.SetRefreshToken(FakeGaiaMixin::kFakeRefreshToken);
  login_manager_.LoginAndWaitForActiveSession(user_context);
}

void LoggedInUserMixin::set_should_launch_browser(bool value) {
  login_manager_.set_should_launch_browser(value);
}

}  // namespace chromeos
