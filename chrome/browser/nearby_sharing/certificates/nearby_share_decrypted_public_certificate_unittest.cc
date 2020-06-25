// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/certificates/nearby_share_decrypted_public_certificate.h"

#include "base/optional.h"
#include "chrome/browser/nearby_sharing/certificates/constants.h"
#include "chrome/browser/nearby_sharing/certificates/test_util.h"
#include "chrome/browser/nearby_sharing/proto/rpc_resources.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(NearbyShareDecryptedPublicCertificateTest, Decrypt) {
  base::Optional<NearbyShareDecryptedPublicCertificate> cert =
      NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
          GetNearbyShareTestPublicCertificate(),
          GetNearbyShareTestEncryptedMetadataKey());
  EXPECT_TRUE(cert);
  EXPECT_EQ(
      base::Time::FromJavaTime(
          GetNearbyShareTestPublicCertificate().start_time().seconds() * 1000),
      cert->not_before());
  EXPECT_EQ(
      base::Time::FromJavaTime(
          GetNearbyShareTestPublicCertificate().end_time().seconds() * 1000),
      cert->not_after());
  EXPECT_EQ(std::vector<uint8_t>(
                GetNearbyShareTestPublicCertificate().secret_id().begin(),
                GetNearbyShareTestPublicCertificate().secret_id().end()),
            cert->id());
  EXPECT_EQ(GetNearbyShareTestMetadata().SerializeAsString(),
            cert->unencrypted_metadata().SerializeAsString());
}

TEST(NearbyShareDecryptedPublicCertificateTest, Decrypt_IncorrectKeyFailure) {
  // Input incorrect metadata encryption key.
  EXPECT_FALSE(NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
      GetNearbyShareTestPublicCertificate(),
      NearbyShareEncryptedMetadataKey(
          std::vector<uint8_t>(kNearbyShareNumBytesMetadataEncryptionKey, 0x00),
          std::vector<uint8_t>(kNearbyShareNumBytesMetadataEncryptionKeySalt,
                               0x00))));
}

TEST(NearbyShareDecryptedPublicCertificateTest,
     Decrypt_MetadataDecryptionFailure) {
  // Use metadata that cannot be decrypted with the given key.
  nearbyshare::proto::PublicCertificate proto_cert =
      GetNearbyShareTestPublicCertificate();
  proto_cert.set_encrypted_metadata_bytes("invalid metadata");
  EXPECT_FALSE(NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
      proto_cert, GetNearbyShareTestEncryptedMetadataKey()));
}

TEST(NearbyShareDecryptedPublicCertificateTest, Decrypt_InvalidDataFailure) {
  // Do not accept the input PublicCertificate because the validity period does
  // not make sense.
  nearbyshare::proto::PublicCertificate proto_cert =
      GetNearbyShareTestPublicCertificate();
  proto_cert.mutable_end_time()->set_seconds(proto_cert.start_time().seconds() -
                                             1);
  EXPECT_FALSE(NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
      proto_cert, GetNearbyShareTestEncryptedMetadataKey()));
}

TEST(NearbySharePublicCertificateTest, Verify) {
  base::Optional<NearbyShareDecryptedPublicCertificate> cert =
      NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
          GetNearbyShareTestPublicCertificate(),
          GetNearbyShareTestEncryptedMetadataKey());
  EXPECT_TRUE(cert->VerifySignature(GetNearbyShareTestPayloadToSign(),
                                    GetNearbyShareTestSampleSignature()));
}

TEST(NearbyShareDecryptedPublicCertificateTest, Verify_InitFailure) {
  // Public key has invalid SubjectPublicKeyInfo format.
  nearbyshare::proto::PublicCertificate proto_cert =
      GetNearbyShareTestPublicCertificate();
  proto_cert.set_public_key("invalid public key");

  base::Optional<NearbyShareDecryptedPublicCertificate> cert =
      NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
          proto_cert, GetNearbyShareTestEncryptedMetadataKey());
  ASSERT_TRUE(cert);
  EXPECT_FALSE(cert->VerifySignature(GetNearbyShareTestPayloadToSign(),
                                     GetNearbyShareTestSampleSignature()));
}

TEST(NearbyShareDecryptedPublicCertificateTest, Verify_WrongSignature) {
  base::Optional<NearbyShareDecryptedPublicCertificate> cert =
      NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
          GetNearbyShareTestPublicCertificate(),
          GetNearbyShareTestEncryptedMetadataKey());
  EXPECT_FALSE(
      cert->VerifySignature(GetNearbyShareTestPayloadToSign(),
                            /*signature=*/base::span<const uint8_t>()));
}
