// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/credential_manager_pending_request_task.h"

#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {
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
    ON_CALL(delegate_mock_, client).WillByDefault(testing::Return(&client_));
  }
  ~CredentialManagerPendingRequestTaskTest() override = default;

 protected:
  testing::NiceMock<CredentialManagerPendingRequestTaskDelegateMock>
      delegate_mock_;

 private:
  StubPasswordManagerClient client_;
};

TEST_F(CredentialManagerPendingRequestTaskTest, QueryProfileStore) {
  CredentialManagerPendingRequestTask task(
      &delegate_mock_, /*callback=*/base::DoNothing(),
      CredentialMediationRequirement::kSilent, /*include_passwords=*/false,
      /*request_federations=*/{}, StoresToQuery::kProfileStore);

  // We are expecting results from only one store, delegate should be called
  // upon getting a response from the store.
  EXPECT_CALL(delegate_mock_, SendCredential);
  task.OnGetPasswordStoreResults({});
}

TEST_F(CredentialManagerPendingRequestTaskTest, QueryProfileAndAccountStores) {
  CredentialManagerPendingRequestTask task(
      &delegate_mock_, /*callback=*/base::DoNothing(),
      CredentialMediationRequirement::kSilent, /*include_passwords=*/false,
      /*request_federations=*/{}, StoresToQuery::kProfileAndAccountStores);

  // We are expecting results from 2 stores, the delegate shouldn't be called
  // until both stores respond.
  EXPECT_CALL(delegate_mock_, SendCredential).Times(0);
  task.OnGetPasswordStoreResults({});

  testing::Mock::VerifyAndClearExpectations(&delegate_mock_);

  EXPECT_CALL(delegate_mock_, SendCredential);
  task.OnGetPasswordStoreResults({});
}

}  // namespace password_manager
