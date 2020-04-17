// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/account_storage_auth_helper.h"

#include <string>

#include "base/callback.h"
#include "build/build_config.h"
#include "chrome/browser/signin/reauth_result.h"
#include "chrome/browser/ui/signin_view_controller.h"
#include "components/signin/public/base/signin_metrics.h"
#include "google_apis/gaia/core_account_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockSigninViewController : public SigninViewController {
 public:
  MockSigninViewController() : SigninViewController(/*browser=*/nullptr) {}
  ~MockSigninViewController() override = default;

  MOCK_METHOD(void,
              ShowReauthPrompt,
              (const CoreAccountId&,
               base::OnceCallback<void(signin::ReauthResult)>),
              (override));

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  MOCK_METHOD(void,
              ShowDiceAddAccountTab,
              (signin_metrics::AccessPoint, const std::string&),
              (override));
#endif
};

}  // namespace

class AccountStorageAuthHelperTest : public ::testing::Test {
 public:
  AccountStorageAuthHelperTest() = default;
  ~AccountStorageAuthHelperTest() override = default;

 protected:
  MockSigninViewController mock_signin_view_controller_;
};
