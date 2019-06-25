// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/credential_management_handler.h"

#include <memory>

#include "base/bind.h"
#include "base/test/scoped_task_environment.h"
#include "device/fido/credential_management.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/test_callback_receiver.h"
#include "device/fido/virtual_fido_device_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {
namespace {

constexpr char kPIN[] = "1234";
constexpr uint8_t kCredentialID[] = {0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa,
                                     0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa};
constexpr char kRPID[] = "example.com";
constexpr uint8_t kUserID[] = {0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
                               0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1};
constexpr char kUserName[] = "alice@example.com";
constexpr char kUserDisplayName[] = "Alice Example <alice@example.com>";

class CredentialManagementHandlerTest : public ::testing::Test {
 protected:
  std::unique_ptr<CredentialManagementHandler> MakeHandler() {
    auto handler = std::make_unique<CredentialManagementHandler>(
        /*connector=*/nullptr, &virtual_device_factory_,
        base::flat_set<FidoTransportProtocol>{
            FidoTransportProtocol::kUsbHumanInterfaceDevice},
        ready_callback_.callback(),
        base::BindRepeating(&CredentialManagementHandlerTest::GetPIN,
                            base::Unretained(this)),
        finished_callback_.callback());
    return handler;
  }

  void GetPIN(int64_t num_attempts,
              base::OnceCallback<void(std::string)> provide_pin) {
    std::move(provide_pin).Run(kPIN);
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;

  test::TestCallbackReceiver<> ready_callback_;
  test::StatusAndValuesCallbackReceiver<
      CtapDeviceResponseCode,
      base::Optional<std::vector<AggregatedEnumerateCredentialsResponse>>,
      base::Optional<size_t>>
      get_credentials_callback_;
  test::ValueCallbackReceiver<CtapDeviceResponseCode> delete_callback_;
  test::ValueCallbackReceiver<FidoReturnCode> finished_callback_;
  test::VirtualFidoDeviceFactory virtual_device_factory_;
};

TEST_F(CredentialManagementHandlerTest, Test) {
  VirtualCtap2Device::Config ctap_config;
  ctap_config.pin_support = true;
  ctap_config.resident_key_support = true;
  ctap_config.credential_management_support = true;
  ctap_config.resident_credential_storage = 100;
  virtual_device_factory_.SetCtap2Config(ctap_config);
  virtual_device_factory_.SetSupportedProtocol(device::ProtocolVersion::kCtap2);
  virtual_device_factory_.mutable_state()->pin = kPIN;
  virtual_device_factory_.mutable_state()->retries = 8;
  ASSERT_TRUE(virtual_device_factory_.mutable_state()->InjectResidentKey(
      kCredentialID, kRPID, kUserID, kUserName, kUserDisplayName));

  auto handler = MakeHandler();
  ready_callback_.WaitForCallback();

  handler->GetCredentials(get_credentials_callback_.callback());
  get_credentials_callback_.WaitForCallback();

  auto result = get_credentials_callback_.TakeResult();
  ASSERT_EQ(std::get<0>(result), CtapDeviceResponseCode::kSuccess);
  auto opt_response = std::move(std::get<1>(result));
  ASSERT_TRUE(opt_response);
  EXPECT_EQ(opt_response->size(), 1u);
  auto num_remaining = std::get<2>(result);
  ASSERT_TRUE(num_remaining);
  EXPECT_EQ(*num_remaining, 99u);

  handler->DeleteCredential(
      opt_response->front().credentials.front().credential_id,
      delete_callback_.callback());

  delete_callback_.WaitForCallback();
  ASSERT_EQ(CtapDeviceResponseCode::kSuccess, delete_callback_.value());
  EXPECT_EQ(virtual_device_factory_.mutable_state()->registrations.size(), 0u);
  EXPECT_FALSE(finished_callback_.was_called());
}

}  // namespace
}  // namespace device
