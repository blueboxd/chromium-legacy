// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/identity_test_environment.h"
#include "base/bind.h"
#include "base/test/scoped_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace identity {

class IdentityTestEnvironmentTest : public testing::Test {
 public:
  IdentityTestEnvironmentTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::DEFAULT,
            base::test::ScopedTaskEnvironment::ThreadPoolExecutionMode::
                QUEUED) {}

  ~IdentityTestEnvironmentTest() override {
    scoped_task_environment_.RunUntilIdle();
  }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  DISALLOW_COPY_AND_ASSIGN(IdentityTestEnvironmentTest);
};

TEST_F(IdentityTestEnvironmentTest,
       IdentityTestEnvironmentCancelsPendingRequestsOnDestruction) {
  std::unique_ptr<identity::IdentityTestEnvironment> identity_test_environment =
      std::make_unique<identity::IdentityTestEnvironment>();

  identity_test_environment->MakePrimaryAccountAvailable("primary@example.com");
  AccessTokenFetcher::TokenCallback callback = base::BindOnce(
      [](GoogleServiceAuthError error, AccessTokenInfo access_token_info) {});
  std::set<std::string> scopes{"scope"};

  std::unique_ptr<identity::AccessTokenFetcher> fetcher =
      identity_test_environment->identity_manager()
          ->CreateAccessTokenFetcherForAccount(
              identity_test_environment->identity_manager()
                  ->GetPrimaryAccountId(),
              "dummy_consumer", scopes, std::move(callback),
              AccessTokenFetcher::Mode::kImmediate);

  // Deleting the IdentityTestEnvironment should cancel any pending
  // task in order to avoid use-after-free crashes. The destructor of
  // the test will spin the runloop which would run
  // IdentityTestEnvironment pending tasks if not canceled.
  identity_test_environment.reset();
  fetcher.reset();
}

}  // namespace identity
