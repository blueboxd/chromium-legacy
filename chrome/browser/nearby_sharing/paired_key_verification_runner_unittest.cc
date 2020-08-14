// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/paired_key_verification_runner.h"

#include <stdint.h>
#include <vector>

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_certificate_manager.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_decrypted_public_certificate.h"
#include "chrome/browser/nearby_sharing/certificates/test_util.h"
#include "chrome/browser/nearby_sharing/fake_nearby_connection.h"
#include "chrome/browser/nearby_sharing/incoming_frames_reader.h"
#include "chrome/browser/nearby_sharing/mock_nearby_process_manager.h"
#include "chrome/browser/nearby_sharing/share_target.h"
#include "chrome/services/sharing/public/proto/wire_format.pb.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kEndpointId[] = "test_endpoint_id";
const std::vector<uint8_t> kAuthToken = {0, 1, 2};

const std::vector<uint8_t> kPrivateCertificateHashAuthToken = {
    0x8b, 0xcb, 0xa2, 0xf8, 0xe4, 0x06};
const std::vector<uint8_t> kIncomingConnectionSignedData = {
    0x30, 0x45, 0x02, 0x20, 0x4f, 0x83, 0x72, 0xbd, 0x02, 0x70, 0xd9, 0xda,
    0x62, 0x83, 0x5d, 0xb2, 0xdc, 0x6e, 0x3f, 0xa6, 0xa8, 0xa1, 0x4f, 0x5f,
    0xd3, 0xe3, 0xd9, 0x1a, 0x5d, 0x2d, 0x61, 0xd2, 0x6c, 0xdd, 0x8d, 0xa5,
    0x02, 0x21, 0x00, 0xd4, 0xe1, 0x1d, 0x14, 0xcb, 0x58, 0xf7, 0x02, 0xd5,
    0xab, 0x48, 0xe2, 0x2f, 0xcb, 0xc0, 0x53, 0x41, 0x06, 0x50, 0x65, 0x95,
    0x19, 0xa9, 0x22, 0x92, 0x00, 0x42, 0x01, 0x26, 0x25, 0xcb, 0x8c};

const base::TimeDelta kTimeout = base::TimeDelta::FromSeconds(1);

class MockIncomingFramesReader : public IncomingFramesReader {
 public:
  MockIncomingFramesReader(NearbyProcessManager* process_manager,
                           Profile* profile,
                           NearbyConnection* connection)
      : IncomingFramesReader(process_manager, profile, connection) {}

  MOCK_METHOD(void,
              ReadFrame,
              (base::OnceCallback<
                  void(base::Optional<sharing::mojom::V1FramePtr>)> callback),
              (override));

  MOCK_METHOD(
      void,
      ReadFrame,
      (sharing::mojom::V1Frame::Tag frame_type,
       base::OnceCallback<void(base::Optional<sharing::mojom::V1FramePtr>)>
           callback,
       base::TimeDelta timeout),
      (override));
};

class MockNearbyShareCertificateManager : public NearbyShareCertificateManager {
 public:
  MOCK_METHOD(NearbySharePrivateCertificate,
              GetValidPrivateCertificate,
              (NearbyShareVisibility visibility),
              (override));
  MOCK_METHOD(void,
              GetDecryptedPublicCertificate,
              (base::span<const uint8_t> encrypted_metadata_key,
               base::span<const uint8_t> salt,
               CertDecryptedCallback callback),
              (override));
  MOCK_METHOD(void, DownloadPublicCertificates, (), (override));

 protected:
  MOCK_METHOD(void, OnStart, (), (override));
  MOCK_METHOD(void, OnStop, (), (override));
};

PairedKeyVerificationRunner::PairedKeyVerificationResult Merge(
    PairedKeyVerificationRunner::PairedKeyVerificationResult local_result,
    sharing::mojom::PairedKeyResultFrame::Status remote_result) {
  if (remote_result == sharing::mojom::PairedKeyResultFrame_Status::kFail ||
      local_result ==
          PairedKeyVerificationRunner::PairedKeyVerificationResult::kFail) {
    return PairedKeyVerificationRunner::PairedKeyVerificationResult::kFail;
  }

  if (remote_result == sharing::mojom::PairedKeyResultFrame_Status::kSuccess &&
      local_result ==
          PairedKeyVerificationRunner::PairedKeyVerificationResult::kSuccess) {
    return PairedKeyVerificationRunner::PairedKeyVerificationResult::kSuccess;
  }

  return PairedKeyVerificationRunner::PairedKeyVerificationResult::kUnable;
}

}  // namespace

class PairedKeyVerificationRunnerTest : public testing::Test {
 public:
  enum class ReturnFrameType {
    // Return base::nullopt for the frame.
    kNull,
    // Return an empty frame.
    kEmpty,
    // Return a valid frame.
    kValid,
  };

  PairedKeyVerificationRunnerTest()
      : frames_reader_(&mock_nearby_process_manager_, &profile_, &connection_) {
  }

  void SetUp() override {
    share_target.is_incoming = true;

    EXPECT_CALL(certificate_manager_, GetValidPrivateCertificate(testing::_))
        .WillRepeatedly(testing::Return(GetNearbyShareTestPrivateCertificate(
            NearbyShareVisibility::kAllContacts)));
  }

  void SetUpPairedKeyEncryptionFrame(ReturnFrameType frame_type) {
    EXPECT_CALL(
        frames_reader_,
        ReadFrame(
            testing::Eq(sharing::mojom::V1Frame::Tag::PAIRED_KEY_ENCRYPTION),
            testing::_, testing::Eq(kTimeout)))
        .WillOnce(testing::WithArg<1>(testing::Invoke(
            [frame_type](
                base::OnceCallback<void(
                    base::Optional<sharing::mojom::V1FramePtr>)> callback) {
              if (frame_type == ReturnFrameType::kNull) {
                std::move(callback).Run(base::nullopt);
                return;
              }

              sharing::mojom::V1FramePtr mojo_v1frame =
                  sharing::mojom::V1Frame::New();

              if (frame_type == ReturnFrameType::kValid) {
                mojo_v1frame->set_paired_key_encryption(
                    sharing::mojom::PairedKeyEncryptionFrame::New(
                        kIncomingConnectionSignedData,
                        kPrivateCertificateHashAuthToken));
              } else {
                mojo_v1frame->set_paired_key_encryption(
                    sharing::mojom::PairedKeyEncryptionFrame::New());
              }

              std::move(callback).Run(std::move(mojo_v1frame));
            })));
  }

  void SetUpPairedKeyResultFrame(
      ReturnFrameType frame_type,
      sharing::mojom::PairedKeyResultFrame::Status status =
          sharing::mojom::PairedKeyResultFrame_Status::kUnknown) {
    EXPECT_CALL(
        frames_reader_,
        ReadFrame(testing::Eq(sharing::mojom::V1Frame::Tag::PAIRED_KEY_RESULT),
                  testing::_, testing::Eq(kTimeout)))
        .WillOnce(testing::WithArg<1>(testing::Invoke(
            [=](base::OnceCallback<void(
                    base::Optional<sharing::mojom::V1FramePtr>)> callback) {
              if (frame_type == ReturnFrameType::kNull) {
                std::move(callback).Run(base::nullopt);
                return;
              }

              sharing::mojom::V1FramePtr mojo_v1frame =
                  sharing::mojom::V1Frame::New();
              mojo_v1frame->set_paired_key_result(
                  sharing::mojom::PairedKeyResultFrame::New(status));

              std::move(callback).Run(std::move(mojo_v1frame));
            })));
  }

  sharing::nearby::Frame GetWrittenFrame() {
    std::vector<uint8_t> data = connection_.GetWrittenData();
    sharing::nearby::Frame frame;
    frame.ParseFromArray(data.data(), data.size());
    return frame;
  }

  void ExpectPairedKeyEncryptionFrameSent() {
    sharing::nearby::Frame frame = GetWrittenFrame();
    ASSERT_TRUE(frame.has_v1());
    ASSERT_TRUE(frame.v1().has_paired_key_encryption());
  }

  void ExpectCertificateInfoSent() {
    // TODO - Uncomment when crbug.com/1114765 is resolved.
    // sharing::nearby::Frame frame = GetWrittenFrame();
    // ASSERT_TRUE(frame.has_v1());
    // ASSERT_TRUE(frame.v1().has_certificate_info());
  }

  void ExpectPairedKeyResultFrameSent(
      sharing::nearby::PairedKeyResultFrame::Status status) {
    sharing::nearby::Frame frame = GetWrittenFrame();
    ASSERT_TRUE(frame.has_v1());
    ASSERT_TRUE(frame.v1().has_paired_key_result());
    EXPECT_EQ(status, frame.v1().paired_key_result().status());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  testing::NiceMock<MockNearbyProcessManager> mock_nearby_process_manager_;
  TestingProfile profile_;
  FakeNearbyConnection connection_;
  testing::NiceMock<MockNearbyShareCertificateManager> certificate_manager_;
  testing::NiceMock<MockIncomingFramesReader> frames_reader_;
  ShareTarget share_target;
};

TEST_F(PairedKeyVerificationRunnerTest,
       NullCertificate_InvalidPairedKeyEncryptionFrame_RestrictToContacts) {
  PairedKeyVerificationRunner runner(
      share_target, kEndpointId, kAuthToken, &connection_,
      /*certificate=*/base::nullopt, &certificate_manager_,
      nearby_share::mojom::Visibility::kAllContacts,
      /*restrict_to_contacts=*/true, &frames_reader_, kTimeout);

  // Empty key encryption frame fails the certificate verification.
  SetUpPairedKeyEncryptionFrame(ReturnFrameType::kEmpty);

  base::RunLoop run_loop;
  runner.Run(base::BindLambdaForTesting(
      [&](PairedKeyVerificationRunner::PairedKeyVerificationResult result) {
        EXPECT_EQ(
            PairedKeyVerificationRunner::PairedKeyVerificationResult::kFail,
            result);
        run_loop.Quit();
      }));
  run_loop.Run();

  ExpectPairedKeyEncryptionFrameSent();
}

TEST_F(PairedKeyVerificationRunnerTest,
       ValidPairedKeyEncryptionFrame_ResultFrameTimedOut) {
  PairedKeyVerificationRunner runner(
      share_target, kEndpointId, kAuthToken, &connection_,
      NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
          GetNearbyShareTestPublicCertificate(),
          GetNearbyShareTestEncryptedMetadataKey()),
      &certificate_manager_, nearby_share::mojom::Visibility::kAllContacts,
      /*restrict_to_contacts=*/false, &frames_reader_, kTimeout);

  SetUpPairedKeyEncryptionFrame(ReturnFrameType::kValid);

  // Null result frame fails the certificate verification process.
  SetUpPairedKeyResultFrame(ReturnFrameType::kNull);

  base::RunLoop run_loop;
  runner.Run(base::BindLambdaForTesting(
      [&](PairedKeyVerificationRunner::PairedKeyVerificationResult result) {
        EXPECT_EQ(
            PairedKeyVerificationRunner::PairedKeyVerificationResult::kFail,
            result);
        run_loop.Quit();
      }));
  run_loop.Run();

  ExpectPairedKeyEncryptionFrameSent();
  ExpectPairedKeyResultFrameSent(sharing::nearby::PairedKeyResultFrame::UNABLE);
}

struct TestParameters {
  bool is_target_known;
  bool is_valid_certificate;
  PairedKeyVerificationRunnerTest::ReturnFrameType encryption_frame_type;
  PairedKeyVerificationRunner::PairedKeyVerificationResult result;
} kParameters[] = {
    {true, true, PairedKeyVerificationRunnerTest::ReturnFrameType::kValid,
     PairedKeyVerificationRunner::PairedKeyVerificationResult::kSuccess},
    {true, true, PairedKeyVerificationRunnerTest::ReturnFrameType::kEmpty,
     PairedKeyVerificationRunner::PairedKeyVerificationResult::kFail},
    {true, false, PairedKeyVerificationRunnerTest::ReturnFrameType::kValid,
     PairedKeyVerificationRunner::PairedKeyVerificationResult::kUnable},
    {true, false, PairedKeyVerificationRunnerTest::ReturnFrameType::kEmpty,
     PairedKeyVerificationRunner::PairedKeyVerificationResult::kUnable},
    {false, true, PairedKeyVerificationRunnerTest::ReturnFrameType::kValid,
     PairedKeyVerificationRunner::PairedKeyVerificationResult::kUnable},
};

using KeyVerificationTestParam =
    std::tuple<TestParameters, sharing::mojom::PairedKeyResultFrame_Status>;

class ParameterisedPairedKeyVerificationRunnerTest
    : public PairedKeyVerificationRunnerTest,
      public testing::WithParamInterface<KeyVerificationTestParam> {};

TEST_P(ParameterisedPairedKeyVerificationRunnerTest,
       ValidEncryptionFrame_ValidResultFrame) {
  const TestParameters& params = std::get<0>(GetParam());
  sharing::mojom::PairedKeyResultFrame::Status status = std::get<1>(GetParam());
  PairedKeyVerificationRunner::PairedKeyVerificationResult expected_result =
      Merge(params.result, status);

  share_target.is_known = params.is_target_known;
  base::Optional<NearbyShareDecryptedPublicCertificate> certificate;
  if (params.is_valid_certificate) {
    certificate =
        NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
            GetNearbyShareTestPublicCertificate(),
            GetNearbyShareTestEncryptedMetadataKey());
  }

  PairedKeyVerificationRunner runner(
      share_target, kEndpointId, kAuthToken, &connection_,
      std::move(certificate), &certificate_manager_,
      nearby_share::mojom::Visibility::kAllContacts,
      /*restricted_to_contacts=*/false, &frames_reader_, kTimeout);

  SetUpPairedKeyEncryptionFrame(params.encryption_frame_type);
  SetUpPairedKeyResultFrame(
      PairedKeyVerificationRunnerTest::ReturnFrameType::kValid, status);

  base::RunLoop run_loop;
  runner.Run(base::BindLambdaForTesting(
      [&](PairedKeyVerificationRunner::PairedKeyVerificationResult result) {
        EXPECT_EQ(expected_result, result);
        run_loop.Quit();
      }));
  run_loop.Run();

  ExpectPairedKeyEncryptionFrameSent();
  if (params.encryption_frame_type ==
      PairedKeyVerificationRunnerTest::ReturnFrameType::kValid)
    ExpectCertificateInfoSent();

  // Check for result frame sent.
  if (!params.is_valid_certificate) {
    ExpectPairedKeyResultFrameSent(
        sharing::nearby::PairedKeyResultFrame::UNABLE);
    return;
  }

  if (params.encryption_frame_type ==
      PairedKeyVerificationRunnerTest::ReturnFrameType::kEmpty) {
    ExpectPairedKeyResultFrameSent(sharing::nearby::PairedKeyResultFrame::FAIL);
    return;
  }

  if (params.is_target_known) {
    ExpectPairedKeyResultFrameSent(
        sharing::nearby::PairedKeyResultFrame::SUCCESS);
  } else {
    ExpectPairedKeyResultFrameSent(
        sharing::nearby::PairedKeyResultFrame::UNABLE);
  }
}

INSTANTIATE_TEST_SUITE_P(
    /*no prefix*/,
    ParameterisedPairedKeyVerificationRunnerTest,
    testing::Combine(
        testing::ValuesIn(kParameters),
        testing::Values(sharing::mojom::PairedKeyResultFrame_Status::kUnknown,
                        sharing::mojom::PairedKeyResultFrame_Status::kSuccess,
                        sharing::mojom::PairedKeyResultFrame_Status::kFail,
                        sharing::mojom::PairedKeyResultFrame_Status::kUnable)));
