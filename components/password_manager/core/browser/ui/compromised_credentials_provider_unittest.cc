// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/compromised_credentials_provider.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/compromised_credentials_table.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/sync/model/syncable_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

using ::testing::ElementsAreArray;
using ::testing::IsEmpty;

struct MockCompromisedCredentialsProviderObserver
    : CompromisedCredentialsProvider::Observer {
  MOCK_METHOD(void,
              OnCompromisedCredentialsChanged,
              (CompromisedCredentialsProvider::CredentialsView),
              (override));
};

using StrictMockCompromisedCredentialsProviderObserver =
    ::testing::StrictMock<MockCompromisedCredentialsProviderObserver>;

class CompromisedCredentialsProviderTest : public ::testing::Test {
 protected:
  CompromisedCredentialsProviderTest() {
    store_->Init(syncer::SyncableService::StartSyncFlare(), /*prefs=*/nullptr);
  }

  ~CompromisedCredentialsProviderTest() override {
    store_->ShutdownOnUIThread();
    task_env_.RunUntilIdle();
  }

  TestPasswordStore& store() { return *store_; }
  CompromisedCredentialsProvider& provider() { return provider_; }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }

 private:
  base::test::SingleThreadTaskEnvironment task_env_;
  scoped_refptr<TestPasswordStore> store_ =
      base::MakeRefCounted<TestPasswordStore>();
  CompromisedCredentialsProvider provider_{store_};
};

}  // namespace

// Tests whether adding and removing an observer works as expected.
TEST_F(CompromisedCredentialsProviderTest, NotifyObservers) {
  std::vector<CompromisedCredentials> credentials = {CompromisedCredentials()};

  StrictMockCompromisedCredentialsProviderObserver observer;
  provider().AddObserver(&observer);

  // Adding a credential should notify observers. Furthermore, the credential
  // should be present of the list that is passed along.
  EXPECT_CALL(observer,
              OnCompromisedCredentialsChanged(ElementsAreArray(credentials)));
  store().AddCompromisedCredentials(credentials[0]);
  RunUntilIdle();
  EXPECT_THAT(store().compromised_credentials(), ElementsAreArray(credentials));

  // Adding the exact same credential should not result in a notification, as
  // the database is not actually modified.
  EXPECT_CALL(observer, OnCompromisedCredentialsChanged).Times(0);
  store().AddCompromisedCredentials(credentials[0]);
  RunUntilIdle();

  // Remove should notify, and observers should be passed an empty list.
  EXPECT_CALL(observer, OnCompromisedCredentialsChanged(IsEmpty()));
  store().RemoveCompromisedCredentials(
      credentials[0].signon_realm, credentials[0].username,
      RemoveCompromisedCredentialsReason::kRemove);
  RunUntilIdle();
  EXPECT_THAT(store().compromised_credentials(), IsEmpty());

  // Similarly to repeated add, a repeated remove should not notify either.
  EXPECT_CALL(observer, OnCompromisedCredentialsChanged).Times(0);
  store().RemoveCompromisedCredentials(
      credentials[0].signon_realm, credentials[0].username,
      RemoveCompromisedCredentialsReason::kRemove);
  RunUntilIdle();

  // After an observer is removed it should no longer receive notifications.
  provider().RemoveObserver(&observer);
  EXPECT_CALL(observer, OnCompromisedCredentialsChanged).Times(0);
  store().AddCompromisedCredentials(credentials[0]);
  RunUntilIdle();
  EXPECT_THAT(store().compromised_credentials(), ElementsAreArray(credentials));
}

}  // namespace password_manager
