// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/bulk_leak_check_service.h"

#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/leak_detection/mock_leak_detection_check_factory.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

class BulkLeakCheckServiceTest : public testing::Test {
 public:
  BulkLeakCheckServiceTest()
      : service_(identity_test_env_.identity_manager(),
                 base::MakeRefCounted<network::TestSharedURLLoaderFactory>()) {
    service_.set_leak_factory(
        std::make_unique<MockLeakDetectionCheckFactory>());
  }
  ~BulkLeakCheckServiceTest() override { service_.Shutdown(); }

  BulkLeakCheckService& service() { return service_; }

 private:
  base::test::TaskEnvironment task_env_;
  signin::IdentityTestEnvironment identity_test_env_;
  BulkLeakCheckService service_;
};

TEST_F(BulkLeakCheckServiceTest, OnCreation) {
  EXPECT_EQ(0u, service().GetPendingChecksCount());
  EXPECT_EQ(BulkLeakCheckService::State::kIdle, service().state());
}

}  // namespace
}  // namespace password_manager
