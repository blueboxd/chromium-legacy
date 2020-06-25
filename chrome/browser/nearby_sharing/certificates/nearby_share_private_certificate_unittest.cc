// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/optional.h"
#include "chrome/browser/nearby_sharing/certificates/constants.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_decrypted_public_certificate.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_encrypted_metadata_key.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_private_certificate.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_visibility.h"
#include "chrome/browser/nearby_sharing/certificates/test_util.h"
#include "chrome/browser/nearby_sharing/proto/rpc_resources.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(NearbySharePrivateCertificateTest, Construction) {
  NearbySharePrivateCertificate private_certificate(
      NearbyShareVisibility::kAllContacts, GetNearbyShareTestNotBefore(),
      GetNearbyShareTestMetadata());
  EXPECT_EQ(kNearbyShareNumBytesCertificateId, private_certificate.id().size());
  EXPECT_EQ(NearbyShareVisibility::kAllContacts,
            private_certificate.visibility());
  EXPECT_EQ(GetNearbyShareTestNotBefore(), private_certificate.not_before());
  EXPECT_EQ(
      GetNearbyShareTestNotBefore() + kNearbyShareCertificateValidityPeriod,
      private_certificate.not_after());
  EXPECT_EQ(GetNearbyShareTestMetadata().SerializeAsString(),
            private_certificate.unencrypted_metadata().SerializeAsString());
}

TEST(NearbySharePrivateCertificateTest, EncryptMetadataKey) {
  NearbySharePrivateCertificate private_certificate(
      NearbyShareVisibility::kAllContacts, GetNearbyShareTestNotBefore(),
      GetNearbyShareTestMetadata());
  base::Optional<NearbyShareEncryptedMetadataKey> encrypted_metadata_key =
      private_certificate.EncryptMetadataKey();
  ASSERT_TRUE(encrypted_metadata_key);
  EXPECT_EQ(kNearbyShareNumBytesMetadataEncryptionKey,
            encrypted_metadata_key->encrypted_key().size());
  EXPECT_EQ(kNearbyShareNumBytesMetadataEncryptionKeySalt,
            encrypted_metadata_key->salt().size());
}

TEST(NearbySharePrivateCertificateTest, EncryptMetadataKey_FixedData) {
  NearbySharePrivateCertificate private_certificate =
      GetNearbyShareTestPrivateCertificate();
  base::Optional<NearbyShareEncryptedMetadataKey> encrypted_metadata_key =
      private_certificate.EncryptMetadataKey();
  EXPECT_EQ(GetNearbyShareTestEncryptedMetadataKey().encrypted_key(),
            encrypted_metadata_key->encrypted_key());
  EXPECT_EQ(GetNearbyShareTestEncryptedMetadataKey().salt(),
            encrypted_metadata_key->salt());
}

TEST(NearbySharePrivateCertificateTest,
     EncryptMetadataKey_SaltsExhaustedFailure) {
  NearbySharePrivateCertificate private_certificate =
      GetNearbyShareTestPrivateCertificate();
  for (size_t i = 0; i < kNearbyShareMaxNumMetadataEncryptionKeySalts; ++i) {
    EXPECT_TRUE(private_certificate.EncryptMetadataKey());
  }
  EXPECT_FALSE(private_certificate.EncryptMetadataKey());
}

TEST(NearbySharePrivateCertificateTest,
     EncryptMetadataKey_TooManySaltGenerationRetriesFailure) {
  NearbySharePrivateCertificate private_certificate =
      GetNearbyShareTestPrivateCertificate();
  EXPECT_TRUE(private_certificate.EncryptMetadataKey());
  while (private_certificate.next_salts_for_testing().size() <
         kNearbyShareMaxNumMetadataEncryptionKeySaltGenerationRetries) {
    private_certificate.next_salts_for_testing().push(GetNearbyShareTestSalt());
  }
  EXPECT_FALSE(private_certificate.EncryptMetadataKey());
}

TEST(NearbySharePrivateCertificateTest, PublicCertificateConversion) {
  NearbySharePrivateCertificate private_certificate =
      GetNearbyShareTestPrivateCertificate();
  private_certificate.offset_for_testing() = GetNearbyShareTestValidityOffset();
  base::Optional<nearbyshare::proto::PublicCertificate> public_certificate =
      private_certificate.ToPublicCertificate();
  ASSERT_TRUE(public_certificate);
  EXPECT_EQ(GetNearbyShareTestPublicCertificate().SerializeAsString(),
            public_certificate->SerializeAsString());
}

TEST(NearbySharePrivateCertificateTest, EncryptDecryptRoundtrip) {
  NearbySharePrivateCertificate private_certificate =
      GetNearbyShareTestPrivateCertificate();

  base::Optional<NearbyShareDecryptedPublicCertificate>
      decrypted_public_certificate =
          NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
              *private_certificate.ToPublicCertificate(),
              *private_certificate.EncryptMetadataKey());
  ASSERT_TRUE(decrypted_public_certificate);
  EXPECT_EQ(
      private_certificate.unencrypted_metadata().SerializeAsString(),
      decrypted_public_certificate->unencrypted_metadata().SerializeAsString());
}

TEST(NearbySharePrivateCertificateTest, SignVerifyRoundtrip) {
  NearbySharePrivateCertificate private_certificate =
      GetNearbyShareTestPrivateCertificate();
  base::Optional<std::vector<uint8_t>> signature =
      private_certificate.Sign(GetNearbyShareTestPayloadToSign());
  ASSERT_TRUE(signature);

  base::Optional<NearbyShareDecryptedPublicCertificate>
      decrypted_public_certificate =
          NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
              *private_certificate.ToPublicCertificate(),
              *private_certificate.EncryptMetadataKey());
  EXPECT_TRUE(decrypted_public_certificate->VerifySignature(
      GetNearbyShareTestPayloadToSign(), *signature));
}
