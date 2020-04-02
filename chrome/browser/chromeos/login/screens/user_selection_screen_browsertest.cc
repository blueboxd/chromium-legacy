// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "ash/public/cpp/login_screen_test_api.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/test/test_predicate_waiter.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/cryptohome/fake_cryptohome_client.h"

namespace chromeos {

namespace {

// No consumer user according to BrowserPolicyConnector::IsNonEnterpriseUser.
constexpr char kManagedTestUser[] = "manager@example.com";
constexpr char kManagedTestUserGaiaId[] = "3333333333";

}  // namespace

class UserSelectionScreenTest : public LoginManagerTest {
 public:
  UserSelectionScreenTest()
      : LoginManagerTest(false /* should_launch_browser */,
                         false /* should_initialize_webui */) {
    set_force_webui_login(false);
  }
  ~UserSelectionScreenTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoginManagerTest::SetUpCommandLine(command_line);
    // Enable ARC. Otherwise, the banner would not show.
    command_line->AppendSwitchASCII(switches::kArcAvailability,
                                    "officially-supported");
  }

 protected:
  std::vector<LoginManagerMixin::TestUserInfo> test_users_{CreateUsers()};
  LoginManagerMixin login_manager_mixin_{&mixin_host_, test_users_};

 private:
  static std::vector<LoginManagerMixin::TestUserInfo> CreateUsers() {
    auto users = LoginManagerMixin::CreateRegularUsers(3);
    users.push_back(
        LoginManagerMixin::TestUserInfo(AccountId::FromUserEmailGaiaId(
            kManagedTestUser, kManagedTestUserGaiaId)));
    return users;
  }
  DISALLOW_COPY_AND_ASSIGN(UserSelectionScreenTest);
};

// Test that a banner shows up for known-unmanaged users that need dircrypto
// migration. Also test that no banner shows up for users that may be managed.
IN_PROC_BROWSER_TEST_F(UserSelectionScreenTest, ShowDircryptoMigrationBanner) {
  // No banner for the first user since default is no migration.
  EXPECT_FALSE(ash::LoginScreenTestApi::IsWarningBubbleShown());

  std::unique_ptr<base::HistogramTester> histogram_tester =
      std::make_unique<base::HistogramTester>();
  // Change the needs dircrypto migration response.
  FakeCryptohomeClient::Get()->set_needs_dircrypto_migration(true);

  // Focus the 2nd user pod (consumer).
  ASSERT_TRUE(ash::LoginScreenTestApi::FocusUser(test_users_[1].account_id));

  // Wait for FakeCryptohomeClient to send back the check result.
  test::TestPredicateWaiter(base::BindRepeating([]() {
    // Banner should be shown for the 2nd user (consumer).
    return ash::LoginScreenTestApi::IsWarningBubbleShown();
  })).Wait();
  histogram_tester->ExpectBucketCount("Ash.Login.Login.MigrationBanner", true,
                                      1);

  // Change the needs dircrypto migration response.
  FakeCryptohomeClient::Get()->set_needs_dircrypto_migration(false);
  histogram_tester = std::make_unique<base::HistogramTester>();
  // Focus the 3rd user pod (consumer).
  ASSERT_TRUE(ash::LoginScreenTestApi::FocusUser(test_users_[2].account_id));

  // Wait for FakeCryptohomeClient to send back the check result.
  test::TestPredicateWaiter(base::BindRepeating([]() {
    // Banner should be shown for the 3rd user (consumer).
    return !ash::LoginScreenTestApi::IsWarningBubbleShown();
  })).Wait();
  histogram_tester->ExpectBucketCount("Ash.Login.Login.MigrationBanner", false,
                                      1);

  // Change the needs dircrypto migration response.
  FakeCryptohomeClient::Get()->set_needs_dircrypto_migration(true);
  histogram_tester = std::make_unique<base::HistogramTester>();

  // Focus to the 4th user pod (enterprise).
  ASSERT_TRUE(ash::LoginScreenTestApi::FocusUser(test_users_[3].account_id));

  // Wait for FakeCryptohomeClient to send back the check result.
  test::TestPredicateWaiter(base::BindRepeating([]() {
    // Banner should not be shown for the enterprise user.
    return !ash::LoginScreenTestApi::IsWarningBubbleShown();
  })).Wait();

  // Not recorded for enterprise.
  histogram_tester->ExpectUniqueSample("Ash.Login.Login.MigrationBanner", false,
                                       0);
}

}  // namespace chromeos
