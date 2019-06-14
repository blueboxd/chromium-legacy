// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/test/scoped_task_environment.h"
#include "device/fido/bio/enrollment_handler.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/test_callback_receiver.h"
#include "device/fido/virtual_fido_device_factory.h"

namespace device {
namespace {

constexpr char kPIN[] = "1477";

class BioEnrollmentHandlerTest : public ::testing::Test {
  void SetUp() override {
    virtual_device_factory_.SetSupportedProtocol(ProtocolVersion::kCtap2);
    virtual_device_factory_.mutable_state()->pin = kPIN;
    virtual_device_factory_.mutable_state()->retries = 8;
  }

 protected:
  std::unique_ptr<BioEnrollmentHandler> MakeHandler() {
    return std::make_unique<BioEnrollmentHandler>(
        /*connector=*/nullptr,
        base::flat_set<FidoTransportProtocol>{
            FidoTransportProtocol::kUsbHumanInterfaceDevice},
        ready_callback_.callback(), error_callback_.callback(),
        base::BindRepeating(&BioEnrollmentHandlerTest::GetPIN,
                            base::Unretained(this)),
        &virtual_device_factory_);
  }

  void GetPIN(int64_t attempts,
              base::OnceCallback<void(std::string)> provide_pin) {
    std::move(provide_pin).Run(kPIN);
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  test::TestCallbackReceiver<> ready_callback_;
  test::ValueCallbackReceiver<FidoReturnCode> error_callback_;
  test::VirtualFidoDeviceFactory virtual_device_factory_;
};

// Tests getting authenticator modality without pin auth.
TEST_F(BioEnrollmentHandlerTest, Modality) {
  VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.bio_enrollment_support = true;

  virtual_device_factory_.SetCtap2Config(config);

  auto handler = MakeHandler();
  ready_callback_.WaitForCallback();

  test::StatusAndValueCallbackReceiver<CtapDeviceResponseCode,
                                       base::Optional<BioEnrollmentResponse>>
      cb;
  handler->GetModality(cb.callback());
  cb.WaitForCallback();

  // Operation was successful.
  auto result = cb.TakeResult();
  EXPECT_EQ(std::get<0>(result), CtapDeviceResponseCode::kSuccess);

  // Result is valid.
  auto v = std::move(std::get<1>(result));
  EXPECT_TRUE(v);

  // Result is correct.
  BioEnrollmentResponse expected;
  expected.modality = BioEnrollmentModality::kFingerprint;
  EXPECT_EQ(v, expected);
}

// Tests getting authenticator modality without pin auth.
TEST_F(BioEnrollmentHandlerTest, FingerprintSensorInfo) {
  VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.bio_enrollment_support = true;

  virtual_device_factory_.SetCtap2Config(config);

  auto handler = MakeHandler();
  ready_callback_.WaitForCallback();

  test::StatusAndValueCallbackReceiver<CtapDeviceResponseCode,
                                       base::Optional<BioEnrollmentResponse>>
      cb;
  handler->GetSensorInfo(cb.callback());
  cb.WaitForCallback();

  auto result = cb.TakeResult();
  EXPECT_EQ(std::get<0>(result), CtapDeviceResponseCode::kSuccess);

  auto v = std::move(std::get<1>(result));
  EXPECT_TRUE(v);

  BioEnrollmentResponse expected;
  expected.modality = BioEnrollmentModality::kFingerprint;
  expected.fingerprint_kind = BioEnrollmentFingerprintKind::kTouch;
  expected.max_samples_for_enroll = 7;
  EXPECT_EQ(v, expected);
}

// Tests bio enrollment commands against an authenticator lacking support.
TEST_F(BioEnrollmentHandlerTest, NoBioEnrollmentSupport) {
  VirtualCtap2Device::Config config;
  config.pin_support = true;

  virtual_device_factory_.SetCtap2Config(config);

  auto handler = MakeHandler();
  error_callback_.WaitForCallback();
  EXPECT_EQ(error_callback_.value(),
            FidoReturnCode::kAuthenticatorMissingBioEnrollment);

  // Test unsupported bio-enrollment command.
  test::StatusAndValueCallbackReceiver<CtapDeviceResponseCode,
                                       base::Optional<BioEnrollmentResponse>>
      cb0;
  handler->GetModality(cb0.callback());
  cb0.WaitForCallback();

  EXPECT_EQ(cb0.status(), CtapDeviceResponseCode::kCtap2ErrUnsupportedOption);
  EXPECT_FALSE(cb0.value());

  // Test second command - handler should not crash after last command.
  test::StatusAndValueCallbackReceiver<CtapDeviceResponseCode,
                                       base::Optional<BioEnrollmentResponse>>
      cb1;
  handler->GetSensorInfo(cb1.callback());
  cb1.WaitForCallback();

  EXPECT_EQ(cb1.status(), CtapDeviceResponseCode::kCtap2ErrUnsupportedOption);
  EXPECT_FALSE(cb1.value());
}

// Tests fingerprint enrollment lifecycle.
TEST_F(BioEnrollmentHandlerTest, Enroll) {
  VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.bio_enrollment_support = true;

  virtual_device_factory_.SetCtap2Config(config);

  auto handler = MakeHandler();
  ready_callback_.WaitForCallback();

  test::StatusAndValueCallbackReceiver<CtapDeviceResponseCode,
                                       base::Optional<BioEnrollmentResponse>>
      cb;
  handler->EnrollTemplate(cb.callback());

  cb.WaitForCallback();
  EXPECT_EQ(cb.status(), CtapDeviceResponseCode::kSuccess);

  auto v = std::move(std::get<1>(cb.TakeResult()));
  EXPECT_TRUE(v);

  BioEnrollmentResponse expected;
  expected.last_status = BioEnrollmentSampleStatus::kGood;
  expected.remaining_samples = 0;
  EXPECT_EQ(v, expected);
}

// Tests cancelling fingerprint without an ongoing enrollment.
TEST_F(BioEnrollmentHandlerTest, CancelNoEnroll) {
  VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.bio_enrollment_support = true;

  virtual_device_factory_.SetCtap2Config(config);

  auto handler = MakeHandler();
  ready_callback_.WaitForCallback();

  test::ValueCallbackReceiver<CtapDeviceResponseCode> cb;
  handler->Cancel(cb.callback());
  cb.WaitForCallback();

  EXPECT_EQ(cb.value(), CtapDeviceResponseCode::kSuccess);
}

// Tests enumerating enrollments expecting a list of size 1.
TEST_F(BioEnrollmentHandlerTest, Enumerate) {
  VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.bio_enrollment_support = true;

  virtual_device_factory_.SetCtap2Config(config);

  auto handler = MakeHandler();
  ready_callback_.WaitForCallback();

  test::StatusAndValueCallbackReceiver<CtapDeviceResponseCode,
                                       base::Optional<BioEnrollmentResponse>>
      cb;
  handler->EnumerateTemplates(cb.callback());

  cb.WaitForCallback();
  EXPECT_EQ(cb.status(), CtapDeviceResponseCode::kSuccess);

  auto v = std::move(std::get<1>(cb.TakeResult()));
  EXPECT_TRUE(v);

  BioEnrollmentResponse expected;
  expected.enumerated_ids =
      std::vector<std::pair<std::vector<uint8_t>, std::string>>{
          {{0, 0, 0, 1}, "Template0001"}};
  EXPECT_EQ(v, expected);
}

// Tests renaming an enrollment (success and failure).
TEST_F(BioEnrollmentHandlerTest, Rename) {
  VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.bio_enrollment_support = true;

  virtual_device_factory_.SetCtap2Config(config);

  auto handler = MakeHandler();
  ready_callback_.WaitForCallback();

  // Rename existing enrollment.
  test::ValueCallbackReceiver<CtapDeviceResponseCode> cb0;
  handler->RenameTemplate({0, 0, 0, 1}, "OtherFingerprint1", cb0.callback());

  cb0.WaitForCallback();
  EXPECT_EQ(cb0.value(), CtapDeviceResponseCode::kSuccess);

  // Rename non-existent enrollment.
  test::ValueCallbackReceiver<CtapDeviceResponseCode> cb1;
  handler->RenameTemplate({0, 0, 0, 2}, "OtherFingerprint2", cb1.callback());

  cb1.WaitForCallback();
  EXPECT_EQ(cb1.value(), CtapDeviceResponseCode::kCtap2ErrInvalidOption);
}

}  // namespace
}  // namespace device
