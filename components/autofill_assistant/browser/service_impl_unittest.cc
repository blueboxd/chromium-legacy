// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/service_impl.h"

#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill_assistant/browser/client.h"
#include "components/autofill_assistant/browser/device_context.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/mock_client.h"
#include "components/autofill_assistant/browser/website_login_fetcher.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

using ::testing::Return;

class ServiceImplTest : public ::testing::Test {
 public:
  ServiceImplTest() {
    ON_CALL(mock_client_, GetServerUrl)
        .WillByDefault(Return("https://www.google.com/"));

    service_impl_ = ServiceImpl::Create(nullptr, &mock_client_);
  }

 protected:
  std::string GetClientAccountHash() {
    return service_impl_->GetClientAccountHash();
  }

  MockClient mock_client_;
  std::unique_ptr<ServiceImpl> service_impl_;
};

TEST_F(ServiceImplTest, CreatesValidHashFromEmail) {
  ON_CALL(mock_client_, GetAccountEmailAddress)
      .WillByDefault(Return("john.doe@chromium.org"));
  EXPECT_EQ(GetClientAccountHash(),
            "2c8fa87717fab622bb5cc4d18135fe30dae339efd274b450022d361be92b48c3");
}

}  // namespace autofill_assistant
