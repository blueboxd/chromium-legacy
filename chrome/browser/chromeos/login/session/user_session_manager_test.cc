// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/session/user_session_manager.h"

#include "base/macros.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/login/auth/key.h"
#include "chromeos/login/auth/user_context.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {
constexpr char kFakePassword[] = "p4zzw0r(|";
}

class UserSessionManagerTest : public testing::Test {
 public:
  UserSessionManagerTest() {
    SessionManagerClient::InitializeFake();
    user_session_manager_ = UserSessionManager::GetInstance();
  }

  ~UserSessionManagerTest() override { SessionManagerClient::Shutdown(); }

 protected:
  void InitLoginPassword() {
    user_session_manager_->mutable_user_context_for_testing()->SetPasswordKey(
        Key(kFakePassword));
    EXPECT_FALSE(user_session_manager_->user_context()
                     .GetPasswordKey()
                     ->GetSecret()
                     .empty());
    EXPECT_TRUE(FakeSessionManagerClient::Get()->login_password().empty());
  }

  // Convenience pointer to the singleton, not owned.
  UserSessionManager* user_session_manager_;

  // Allows UserSessionManager to request the NetworkConnectionTracker in its
  // constructor.
  content::TestBrowserThreadBundle thread_bundle_;

  user_manager::ScopedUserManager scoped_user_manager_{
      std::make_unique<user_manager::FakeUserManager>()};

 private:
  DISALLOW_COPY_AND_ASSIGN(UserSessionManagerTest);
};

// Calling VoteForSavingLoginPassword() with |save_password| set to false for
// all |PasswordConsumerService|s should not send the password to SessionManager
// and clear it from the user context.
TEST_F(UserSessionManagerTest, PasswordConsumerService_NoSave) {
  InitLoginPassword();
  user_session_manager_->VoteForSavingLoginPassword(
      UserSessionManager::PasswordConsumingService::kNetwork, false);
  EXPECT_TRUE(FakeSessionManagerClient::Get()->login_password().empty());
  EXPECT_TRUE(user_session_manager_->user_context()
                  .GetPasswordKey()
                  ->GetSecret()
                  .empty());
}

// Calling VoteForSavingLoginPassword() with |save_password| set to true should
// send the password to SessionManager and clear it from the user context.
TEST_F(UserSessionManagerTest, PasswordConsumerService_Save) {
  InitLoginPassword();
  user_session_manager_->VoteForSavingLoginPassword(
      UserSessionManager::PasswordConsumingService::kNetwork, true);
  EXPECT_EQ(kFakePassword, FakeSessionManagerClient::Get()->login_password());
  EXPECT_TRUE(user_session_manager_->user_context()
                  .GetPasswordKey()
                  ->GetSecret()
                  .empty());
}

}  // namespace chromeos
