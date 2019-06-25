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
  config.bio_enrollment_preview_support = true;

  virtual_device_factory_.SetCtap2Config(config);

  auto handler = MakeHandler();
  ready_callback_.WaitForCallback();

  test::StatusAndValueCallbackReceiver<CtapDeviceResponseCode,
                                       base::Optional<BioEnrollmentResponse>>
      cb;
  handler->GetModality(cb.callback());
  cb.WaitForCallback();

  // Operation was successful.
  EXPECT_EQ(cb.status(), CtapDeviceResponseCode::kSuccess);

  // Result is correct.
  BioEnrollmentResponse expected;
  expected.modality = BioEnrollmentModality::kFingerprint;
  EXPECT_EQ(cb.value(), expected);
}

// Tests getting authenticator modality without pin auth.
TEST_F(BioEnrollmentHandlerTest, FingerprintSensorInfo) {
  VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.bio_enrollment_preview_support = true;

  virtual_device_factory_.SetCtap2Config(config);

  auto handler = MakeHandler();
  ready_callback_.WaitForCallback();

  test::StatusAndValueCallbackReceiver<CtapDeviceResponseCode,
                                       base::Optional<BioEnrollmentResponse>>
      cb;
  handler->GetSensorInfo(cb.callback());
  cb.WaitForCallback();

  EXPECT_EQ(cb.status(), CtapDeviceResponseCode::kSuccess);

  BioEnrollmentResponse expected;
  expected.modality = BioEnrollmentModality::kFingerprint;
  expected.fingerprint_kind = BioEnrollmentFingerprintKind::kTouch;
  expected.max_samples_for_enroll = 4;
  EXPECT_EQ(cb.value(), expected);
}

// Tests enrollment handler PIN soft block.
TEST_F(BioEnrollmentHandlerTest, SoftPINBlock) {
  VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.bio_enrollment_preview_support = true;

  virtual_device_factory_.mutable_state()->pin = "1234";
  virtual_device_factory_.SetCtap2Config(config);

  auto handler = MakeHandler();
  error_callback_.WaitForCallback();

  EXPECT_EQ(error_callback_.value(), FidoReturnCode::kSoftPINBlock);
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
}

// Tests fingerprint enrollment lifecycle.
TEST_F(BioEnrollmentHandlerTest, Enroll) {
  VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.bio_enrollment_preview_support = true;

  virtual_device_factory_.SetCtap2Config(config);

  auto handler = MakeHandler();
  ready_callback_.WaitForCallback();

  test::StatusAndValueCallbackReceiver<CtapDeviceResponseCode,
                                       base::Optional<BioEnrollmentResponse>>
      cb;
  handler->EnrollTemplate(cb.callback());

  cb.WaitForCallback();
  EXPECT_EQ(cb.status(), CtapDeviceResponseCode::kSuccess);

  BioEnrollmentResponse expected;
  expected.last_status = BioEnrollmentSampleStatus::kGood;
  expected.remaining_samples = 0;
  EXPECT_EQ(cb.value(), expected);
}

// Tests enrolling multiple fingerprints.
TEST_F(BioEnrollmentHandlerTest, EnrollMultiple) {
  VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.bio_enrollment_preview_support = true;

  virtual_device_factory_.SetCtap2Config(config);

  auto handler = MakeHandler();
  ready_callback_.WaitForCallback();

  // Multiple enrollments
  for (auto i = 0; i < 4; i++) {
    test::StatusAndValueCallbackReceiver<CtapDeviceResponseCode,
                                         base::Optional<BioEnrollmentResponse>>
        cb;
    handler->EnrollTemplate(cb.callback());

    cb.WaitForCallback();
    EXPECT_EQ(cb.status(), CtapDeviceResponseCode::kSuccess);
  }

  // Enumerate to check enrollments.
  test::StatusAndValueCallbackReceiver<CtapDeviceResponseCode,
                                       base::Optional<BioEnrollmentResponse>>
      cb;
  handler->EnumerateTemplates(cb.callback());

  cb.WaitForCallback();
  EXPECT_EQ(cb.status(), CtapDeviceResponseCode::kSuccess);

  BioEnrollmentResponse expected;
  expected.template_infos =
      std::vector<std::pair<std::vector<uint8_t>, std::string>>{
          {{1}, "Template1"},
          {{2}, "Template2"},
          {{3}, "Template3"},
          {{4}, "Template4"}};
  EXPECT_EQ(cb.value(), expected);
}

// Tests enrolling beyond maximum capacity.
TEST_F(BioEnrollmentHandlerTest, EnrollMax) {
  VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.bio_enrollment_preview_support = true;

  virtual_device_factory_.SetCtap2Config(config);

  auto handler = MakeHandler();
  ready_callback_.WaitForCallback();

  // Enroll until full.
  auto status = CtapDeviceResponseCode::kSuccess;
  while (status == CtapDeviceResponseCode::kSuccess) {
    test::StatusAndValueCallbackReceiver<CtapDeviceResponseCode,
                                         base::Optional<BioEnrollmentResponse>>
        cb;
    handler->EnrollTemplate(cb.callback());

    cb.WaitForCallback();
    status = cb.status();
  }

  EXPECT_EQ(status, CtapDeviceResponseCode::kCtap2ErrKeyStoreFull);
}

// Tests cancelling fingerprint without an ongoing enrollment.
TEST_F(BioEnrollmentHandlerTest, CancelNoEnroll) {
  VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.bio_enrollment_preview_support = true;

  virtual_device_factory_.SetCtap2Config(config);

  auto handler = MakeHandler();
  ready_callback_.WaitForCallback();

  test::ValueCallbackReceiver<CtapDeviceResponseCode> cb;
  handler->Cancel(cb.callback());
  cb.WaitForCallback();

  EXPECT_EQ(cb.value(), CtapDeviceResponseCode::kSuccess);
}

// Tests enumerating with no enrollments.
TEST_F(BioEnrollmentHandlerTest, EnumerateNone) {
  VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.bio_enrollment_preview_support = true;

  virtual_device_factory_.SetCtap2Config(config);

  auto handler = MakeHandler();
  ready_callback_.WaitForCallback();

  test::StatusAndValueCallbackReceiver<CtapDeviceResponseCode,
                                       base::Optional<BioEnrollmentResponse>>
      cb;
  handler->EnumerateTemplates(cb.callback());

  cb.WaitForCallback();
  EXPECT_EQ(cb.status(), CtapDeviceResponseCode::kCtap2ErrInvalidOption);
}

// Tests enumerating with one enrollment.
TEST_F(BioEnrollmentHandlerTest, EnumerateOne) {
  VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.bio_enrollment_preview_support = true;

  virtual_device_factory_.SetCtap2Config(config);

  auto handler = MakeHandler();
  ready_callback_.WaitForCallback();

  // Enroll - skip response validation
  test::StatusAndValueCallbackReceiver<CtapDeviceResponseCode,
                                       base::Optional<BioEnrollmentResponse>>
      cb0;
  handler->EnrollTemplate(cb0.callback());

  cb0.WaitForCallback();
  EXPECT_EQ(cb0.status(), CtapDeviceResponseCode::kSuccess);

  // Enumerate
  test::StatusAndValueCallbackReceiver<CtapDeviceResponseCode,
                                       base::Optional<BioEnrollmentResponse>>
      cb1;
  handler->EnumerateTemplates(cb1.callback());

  cb1.WaitForCallback();
  EXPECT_EQ(cb1.status(), CtapDeviceResponseCode::kSuccess);

  BioEnrollmentResponse expected;
  expected.template_infos =
      std::vector<std::pair<std::vector<uint8_t>, std::string>>{
          {{1}, "Template1"}};
  EXPECT_EQ(cb1.value(), expected);
}

// Tests renaming an enrollment (success and failure).
TEST_F(BioEnrollmentHandlerTest, Rename) {
  VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.bio_enrollment_preview_support = true;

  virtual_device_factory_.SetCtap2Config(config);

  auto handler = MakeHandler();
  ready_callback_.WaitForCallback();

  // Rename non-existent enrollment.
  test::ValueCallbackReceiver<CtapDeviceResponseCode> cb0;
  handler->RenameTemplate({1}, "OtherFingerprint1", cb0.callback());

  cb0.WaitForCallback();
  EXPECT_EQ(cb0.value(), CtapDeviceResponseCode::kCtap2ErrInvalidOption);

  // Enroll - skip response validation.
  test::StatusAndValueCallbackReceiver<CtapDeviceResponseCode,
                                       base::Optional<BioEnrollmentResponse>>
      cb1;
  handler->EnrollTemplate(cb1.callback());

  cb1.WaitForCallback();
  EXPECT_EQ(cb1.status(), CtapDeviceResponseCode::kSuccess);

  // Rename non-existent enrollment.
  test::ValueCallbackReceiver<CtapDeviceResponseCode> cb2;
  handler->RenameTemplate({1}, "OtherFingerprint1", cb2.callback());

  cb2.WaitForCallback();
  EXPECT_EQ(cb2.value(), CtapDeviceResponseCode::kSuccess);

  // Enumerate to validate renaming.
  test::StatusAndValueCallbackReceiver<CtapDeviceResponseCode,
                                       base::Optional<BioEnrollmentResponse>>
      cb3;
  handler->EnumerateTemplates(cb3.callback());

  cb3.WaitForCallback();
  EXPECT_EQ(cb3.status(), CtapDeviceResponseCode::kSuccess);

  BioEnrollmentResponse expected;
  expected.template_infos =
      std::vector<std::pair<std::vector<uint8_t>, std::string>>{
          {{1}, "OtherFingerprint1"}};
  EXPECT_EQ(cb3.value(), expected);
}

// Tests deleting an enrollment (success and failure).
TEST_F(BioEnrollmentHandlerTest, Delete) {
  VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.bio_enrollment_preview_support = true;

  virtual_device_factory_.SetCtap2Config(config);

  auto handler = MakeHandler();
  ready_callback_.WaitForCallback();

  // Delete non-existent enrollment.
  test::ValueCallbackReceiver<CtapDeviceResponseCode> cb0;
  handler->DeleteTemplate({1}, cb0.callback());

  cb0.WaitForCallback();
  EXPECT_EQ(cb0.value(), CtapDeviceResponseCode::kCtap2ErrInvalidOption);

  // Enroll - skip response validation.
  test::StatusAndValueCallbackReceiver<CtapDeviceResponseCode,
                                       base::Optional<BioEnrollmentResponse>>
      cb1;
  handler->EnrollTemplate(cb1.callback());

  cb1.WaitForCallback();
  EXPECT_EQ(cb1.status(), CtapDeviceResponseCode::kSuccess);

  // Delete existing enrollment.
  test::ValueCallbackReceiver<CtapDeviceResponseCode> cb2;
  handler->DeleteTemplate({1}, cb2.callback());

  cb2.WaitForCallback();
  EXPECT_EQ(cb2.value(), CtapDeviceResponseCode::kSuccess);

  // Attempt to delete again to prove enrollment is gone.
  test::ValueCallbackReceiver<CtapDeviceResponseCode> cb3;
  handler->DeleteTemplate({1}, cb3.callback());

  cb3.WaitForCallback();
  EXPECT_EQ(cb3.value(), CtapDeviceResponseCode::kCtap2ErrInvalidOption);
}

// Test cancelling using the non-preview command.
TEST_F(BioEnrollmentHandlerTest, NonPreviewCancel) {
  VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.bio_enrollment_support = true;

  virtual_device_factory_.SetCtap2Config(config);

  auto handler = MakeHandler();
  ready_callback_.WaitForCallback();

  // Cancel enrollment.
  test::ValueCallbackReceiver<CtapDeviceResponseCode> cb;
  handler->Cancel(cb.callback());

  cb.WaitForCallback();
  EXPECT_EQ(cb.value(), CtapDeviceResponseCode::kSuccess);
}

}  // namespace
}  // namespace device
