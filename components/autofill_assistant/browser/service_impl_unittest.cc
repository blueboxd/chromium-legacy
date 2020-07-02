// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/service_impl.h"

#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill_assistant/browser/client.h"
#include "components/autofill_assistant/browser/device_context.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/mock_client.h"
#include "components/autofill_assistant/browser/website_login_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

using ::testing::Return;

class ServiceImplTest : public ::testing::Test {
 public:
  ServiceImplTest() {
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
  ON_CALL(mock_client_, GetChromeSignedInEmailAddress)
      .WillByDefault(Return("john.doe@chromium.org"));
  EXPECT_EQ(GetClientAccountHash(),
            "2c8fa87717fab622bb5cc4d18135fe30dae339efd274b450022d361be92b48c3");
}

class ServiceImplTestSignedInStatus
    : public ServiceImplTest,
      public testing::WithParamInterface<
          std::pair<std::string, ClientContextProto::SignedIntoChromeStatus>> {
};

INSTANTIATE_TEST_SUITE_P(
    ParametrizedTests,
    ServiceImplTestSignedInStatus,
    testing::Values(std::make_pair("", ClientContextProto::NOT_SIGNED_IN),
                    std::make_pair("bob@email.com",
                                   ClientContextProto::SIGNED_IN)));

TEST_P(ServiceImplTestSignedInStatus, SetsSignedInStatus) {
  ON_CALL(mock_client_, GetChromeSignedInEmailAddress)
      .WillByDefault(Return(GetParam().first));
  service_impl_->UpdateMutableClientContextFields();
  EXPECT_EQ(service_impl_->client_context_.signed_into_chrome_status(),
            GetParam().second);
}

}  // namespace autofill_assistant
