// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/credential_manager_pending_request_task.h"

#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

class TestPasswordManagerClient : public StubPasswordManagerClient {
 public:
  TestPasswordManagerClient(PasswordStore* profile_store,
                            PasswordStore* account_store)
      : profile_store_(profile_store), account_store_(account_store) {}
  PasswordStore* GetProfilePasswordStore() const override {
    return profile_store_;
  }
  PasswordStore* GetAccountPasswordStore() const override {
    return account_store_;
  }

 private:
  PasswordStore* profile_store_;
  PasswordStore* account_store_;
};

class CredentialManagerPendingRequestTaskDelegateMock
    : public CredentialManagerPendingRequestTaskDelegate {
 public:
  CredentialManagerPendingRequestTaskDelegateMock() = default;
  ~CredentialManagerPendingRequestTaskDelegateMock() = default;

  MOCK_METHOD(bool, IsZeroClickAllowed, (), (const, override));
  MOCK_METHOD(url::Origin, GetOrigin, (), (const, override));
  MOCK_METHOD(PasswordManagerClient*, client, (), (const, override));
  MOCK_METHOD(void,
              SendCredential,
              (SendCredentialCallback send_callback,
               const CredentialInfo& credential),
              (override));
  MOCK_METHOD(void,
              SendPasswordForm,
              (SendCredentialCallback send_callback,
               CredentialMediationRequirement mediation,
               const autofill::PasswordForm* form),
              (override));
};
}  // namespace
class CredentialManagerPendingRequestTaskTest : public ::testing::Test {
 public:
  CredentialManagerPendingRequestTaskTest() {
    profile_store_ = new TestPasswordStore(/*is_account_store=*/false);
    profile_store_->Init(/*prefs=*/nullptr);

    account_store_ = new TestPasswordStore(/*is_account_store=*/true);
    account_store_->Init(/*prefs=*/nullptr);

    client_ = std::make_unique<TestPasswordManagerClient>(profile_store_.get(),
                                                          account_store_.get());

    ON_CALL(delegate_mock_, client)
        .WillByDefault(testing::Return(client_.get()));
  }
  ~CredentialManagerPendingRequestTaskTest() override = default;

  void TearDown() override {
    account_store_->ShutdownOnUIThread();
    profile_store_->ShutdownOnUIThread();
    // It's needed to cleanup the password store asynchronously.
    task_environment_.RunUntilIdle();
  }

 protected:
  testing::NiceMock<CredentialManagerPendingRequestTaskDelegateMock>
      delegate_mock_;
  scoped_refptr<TestPasswordStore> profile_store_;
  scoped_refptr<TestPasswordStore> account_store_;

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestPasswordManagerClient> client_;
};

TEST_F(CredentialManagerPendingRequestTaskTest, QueryProfileStore) {
  CredentialManagerPendingRequestTask task(
      &delegate_mock_, /*callback=*/base::DoNothing(),
      CredentialMediationRequirement::kSilent, /*include_passwords=*/false,
      /*request_federations=*/{}, StoresToQuery::kProfileStore);

  // We are expecting results from only one store, delegate should be called
  // upon getting a response from the store.
  EXPECT_CALL(delegate_mock_, SendCredential);
  task.OnGetPasswordStoreResultsFrom(profile_store_, {});
}

TEST_F(CredentialManagerPendingRequestTaskTest, QueryProfileAndAccountStores) {
  CredentialManagerPendingRequestTask task(
      &delegate_mock_, /*callback=*/base::DoNothing(),
      CredentialMediationRequirement::kSilent, /*include_passwords=*/false,
      /*request_federations=*/{}, StoresToQuery::kProfileAndAccountStores);

  // We are expecting results from 2 stores, the delegate shouldn't be called
  // until both stores respond.
  EXPECT_CALL(delegate_mock_, SendCredential).Times(0);
  task.OnGetPasswordStoreResultsFrom(profile_store_, {});

  testing::Mock::VerifyAndClearExpectations(&delegate_mock_);

  EXPECT_CALL(delegate_mock_, SendCredential);
  task.OnGetPasswordStoreResultsFrom(account_store_, {});
}

}  // namespace password_manager
