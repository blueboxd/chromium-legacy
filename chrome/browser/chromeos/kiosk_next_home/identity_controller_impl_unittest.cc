// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kiosk_next_home/identity_controller_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/chromeos/kiosk_next_home/metrics_helper.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace kiosk_next_home {

namespace {

const char* kUserDisplayName = "User Display Name";
const char* kUserGivenName = "User Given Name";

}  // namespace

class IdentityControllerImplTest : public testing::Test {
 protected:
  void SetUp() override {
    auto* fake_user_manager = new user_manager::FakeUserManager();
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        base::WrapUnique(fake_user_manager));
    AccountId account_id =
        AccountId::FromUserEmailGaiaId("test@test.com", "123456");
    // Create new fake user and set its account data.
    fake_user_manager->AddUser(account_id);
    user_manager::UserManager::UserAccountData account_data(
        base::ASCIIToUTF16(kUserDisplayName),
        base::ASCIIToUTF16(kUserGivenName), std::string() /* locale */);
    fake_user_manager->UpdateUserAccountData(account_id, account_data);
    identity_controller_impl_ = std::make_unique<IdentityControllerImpl>(
        mojo::MakeRequest(&identity_controller_));
  }

  mojom::IdentityController* identity_controller() {
    return identity_controller_.get();
  }

  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedTaskEnvironment task_environemnt_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  std::unique_ptr<IdentityControllerImpl> identity_controller_impl_;
  mojom::IdentityControllerPtr identity_controller_;
};

TEST_F(IdentityControllerImplTest, GetUserInfo) {
  base::RunLoop run_loop;
  mojom::UserInfoPtr returned_user_info;
  identity_controller()->GetUserInfo(base::BindLambdaForTesting(
      [&run_loop, &returned_user_info](mojom::UserInfoPtr user_info) {
        returned_user_info = std::move(user_info);
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_EQ(returned_user_info->display_name, kUserDisplayName);
  EXPECT_EQ(returned_user_info->given_name, kUserGivenName);
  histogram_tester_.ExpectUniqueSample("KioskNextHome.Bridge.Action",
                                       BridgeAction::kGetUserInfo, 1);
}

}  // namespace kiosk_next_home
}  // namespace chromeos
