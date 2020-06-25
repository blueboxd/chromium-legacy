// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/certificates/nearby_share_private_certificate.h"

#include <utility>

#include "base/logging.h"
#include "base/rand_util.h"
#include "chrome/browser/nearby_sharing/certificates/common.h"
#include "chrome/browser/nearby_sharing/certificates/constants.h"
#include "chrome/browser/nearby_sharing/proto/timestamp.pb.h"
#include "crypto/aead.h"
#include "crypto/ec_private_key.h"
#include "crypto/ec_signature_creator.h"
#include "crypto/encryptor.h"
#include "crypto/hmac.h"
#include "crypto/random.h"
#include "crypto/sha2.h"
#include "crypto/symmetric_key.h"

namespace {

std::vector<uint8_t> GenerateRandomBytes(size_t num_bytes) {
  std::vector<uint8_t> bytes(num_bytes);
  crypto::RandBytes(bytes);

  return bytes;
}

// Generates a random validity bound offset in the interval
// [0, kNearbyShareMaxPrivateCertificateValidityBoundOffset).
base::TimeDelta GenerateRandomOffset() {
  return base::TimeDelta::FromMicroseconds(base::RandGenerator(
      kNearbyShareMaxPrivateCertificateValidityBoundOffset.InMicroseconds()));
}

// Generates a certificate identifier by hashing the input secret |key|.
std::vector<uint8_t> CreateCertificateIdFromSecretKey(
    const crypto::SymmetricKey& key) {
  DCHECK_EQ(crypto::kSHA256Length, kNearbyShareNumBytesCertificateId);
  std::vector<uint8_t> id(kNearbyShareNumBytesCertificateId);
  crypto::SHA256HashString(key.key(), id.data(), id.size());

  return id;
}

// Creates an HMAC from |metadata_encryption_key| to be used as a key commitment
// in certificates.
base::Optional<std::vector<uint8_t>> CreateMetadataEncryptionKeyTag(
    base::span<const uint8_t> metadata_encryption_key) {
  // This array of 0x00 is used to conform with the GmsCore implementation.
  std::vector<uint8_t> key(kNearbyShareNumBytesMetadataEncryptionKeyTag, 0x00);

  std::vector<uint8_t> result(kNearbyShareNumBytesMetadataEncryptionKeyTag);
  crypto::HMAC hmac(crypto::HMAC::HashAlgorithm::SHA256);
  if (!hmac.Init(key) || !hmac.Sign(metadata_encryption_key, result))
    return base::nullopt;

  return result;
}

}  // namespace

NearbySharePrivateCertificate::NearbySharePrivateCertificate(
    NearbyShareVisibility visibility,
    base::Time not_before,
    nearbyshare::proto::EncryptedMetadata unencrypted_metadata)
    : visibility_(visibility),
      not_before_(not_before),
      not_after_(not_before_ + kNearbyShareCertificateValidityPeriod),
      key_pair_(crypto::ECPrivateKey::Create()),
      secret_key_(crypto::SymmetricKey::GenerateRandomKey(
          crypto::SymmetricKey::Algorithm::AES,
          /*key_size_in_bits=*/8 * kNearbyShareNumBytesSecretKey)),
      metadata_encryption_key_(
          GenerateRandomBytes(kNearbyShareNumBytesMetadataEncryptionKey)),
      id_(CreateCertificateIdFromSecretKey(*secret_key_)),
      unencrypted_metadata_(std::move(unencrypted_metadata)) {
  DCHECK_NE(visibility, NearbyShareVisibility::kNoOne);
}

NearbySharePrivateCertificate::NearbySharePrivateCertificate(
    NearbyShareVisibility visibility,
    base::Time not_before,
    base::Time not_after,
    std::unique_ptr<crypto::ECPrivateKey> key_pair,
    std::unique_ptr<crypto::SymmetricKey> secret_key,
    std::vector<uint8_t> metadata_encryption_key,
    std::vector<uint8_t> id,
    nearbyshare::proto::EncryptedMetadata unencrypted_metadata,
    std::set<std::vector<uint8_t>> consumed_salts)
    : visibility_(visibility),
      not_before_(not_before),
      not_after_(not_after),
      key_pair_(std::move(key_pair)),
      secret_key_(std::move(secret_key)),
      metadata_encryption_key_(std::move(metadata_encryption_key)),
      id_(std::move(id)),
      unencrypted_metadata_(std::move(unencrypted_metadata)),
      consumed_salts_(std::move(consumed_salts)) {
  DCHECK_NE(visibility, NearbyShareVisibility::kNoOne);
}

NearbySharePrivateCertificate::NearbySharePrivateCertificate(
    NearbySharePrivateCertificate&&) = default;

NearbySharePrivateCertificate::~NearbySharePrivateCertificate() = default;

base::Optional<NearbyShareEncryptedMetadataKey>
NearbySharePrivateCertificate::EncryptMetadataKey() {
  base::Optional<std::vector<uint8_t>> salt = GenerateUnusedSalt();
  if (!salt) {
    LOG(ERROR) << "Encryption failed: Salt generation unsuccessful.";
    return base::nullopt;
  }

  std::unique_ptr<crypto::Encryptor> encryptor =
      CreateNearbyShareCtrEncryptor(secret_key_.get(), *salt);
  if (!encryptor) {
    LOG(ERROR) << "Encryption failed: Could not create CTR encryptor.";
    return base::nullopt;
  }

  DCHECK_EQ(kNearbyShareNumBytesMetadataEncryptionKey,
            metadata_encryption_key_.size());
  std::vector<uint8_t> encrypted_metadata_key;
  if (!encryptor->Encrypt(metadata_encryption_key_, &encrypted_metadata_key)) {
    LOG(ERROR) << "Encryption failed: Could not encrypt metadata key.";
    return base::nullopt;
  }

  return NearbyShareEncryptedMetadataKey(encrypted_metadata_key, *salt);
}

base::Optional<std::vector<uint8_t>> NearbySharePrivateCertificate::Sign(
    base::span<const uint8_t> payload) const {
  std::unique_ptr<crypto::ECSignatureCreator> signer(
      crypto::ECSignatureCreator::Create(key_pair_.get()));

  std::vector<uint8_t> signature;
  if (!signer->Sign(payload.data(), payload.size(), &signature)) {
    LOG(ERROR) << "Signing failed.";
    return base::nullopt;
  }

  return signature;
}

base::Optional<nearbyshare::proto::PublicCertificate>
NearbySharePrivateCertificate::ToPublicCertificate() {
  std::vector<uint8_t> public_key;
  if (!key_pair_->ExportPublicKey(&public_key)) {
    LOG(ERROR) << "Failed to export public key.";
    return base::nullopt;
  }

  base::Optional<std::vector<uint8_t>> encrypted_metadata_bytes =
      EncryptMetadata();
  if (!encrypted_metadata_bytes) {
    LOG(ERROR) << "Failed to encrypt metadata.";
    return base::nullopt;
  }

  base::Optional<std::vector<uint8_t>> metadata_encryption_key_tag =
      CreateMetadataEncryptionKeyTag(metadata_encryption_key_);
  if (!metadata_encryption_key_tag) {
    LOG(ERROR) << "Failed to compute metadata encryption key tag.";
    return base::nullopt;
  }

  base::TimeDelta not_before_offset =
      offset_for_testing_.value_or(GenerateRandomOffset());
  base::TimeDelta not_after_offset =
      offset_for_testing_.value_or(GenerateRandomOffset());

  nearbyshare::proto::PublicCertificate public_certificate;
  public_certificate.set_secret_id(std::string(id_.begin(), id_.end()));
  public_certificate.set_secret_key(secret_key_->key());
  public_certificate.set_public_key(
      std::string(public_key.begin(), public_key.end()));
  public_certificate.mutable_start_time()->set_seconds(
      (not_before_ - not_before_offset).ToJavaTime() / 1000);
  public_certificate.mutable_end_time()->set_seconds(
      (not_after_ + not_after_offset).ToJavaTime() / 1000);
  public_certificate.set_for_selected_contacts(
      visibility_ == NearbyShareVisibility::kSelectedContacts);
  public_certificate.set_metadata_encryption_key(std::string(
      metadata_encryption_key_.begin(), metadata_encryption_key_.end()));
  public_certificate.set_encrypted_metadata_bytes(std::string(
      encrypted_metadata_bytes->begin(), encrypted_metadata_bytes->end()));
  public_certificate.set_metadata_encryption_key_tag(
      std::string(metadata_encryption_key_tag->begin(),
                  metadata_encryption_key_tag->end()));

  return public_certificate;
}

base::Optional<std::vector<uint8_t>>
NearbySharePrivateCertificate::GenerateUnusedSalt() {
  if (consumed_salts_.size() >= kNearbyShareMaxNumMetadataEncryptionKeySalts) {
    LOG(ERROR) << "All salts exhausted for certificate.";
    return base::nullopt;
  }

  for (size_t attempt = 0;
       attempt < kNearbyShareMaxNumMetadataEncryptionKeySaltGenerationRetries;
       ++attempt) {
    std::vector<uint8_t> salt;
    if (next_salts_for_testing_.empty()) {
      salt = GenerateRandomBytes(2u);
    } else {
      salt = next_salts_for_testing_.front();
      next_salts_for_testing_.pop();
    }
    DCHECK_EQ(2u, salt.size());

    if (!base::Contains(consumed_salts_, salt)) {
      consumed_salts_.insert(salt);
      return salt;
    }
  }

  LOG(ERROR) << "Salt generation exceeded max number of retries. This is "
                "highly improbable.";
  return base::nullopt;
}

base::Optional<std::vector<uint8_t>>
NearbySharePrivateCertificate::EncryptMetadata() {
  // Init() keeps a reference to the input key, so that reference must outlive
  // the lifetime of |aead|.
  std::vector<uint8_t> derived_key = DeriveNearbyShareKey(
      metadata_encryption_key_, kNearbyShareNumBytesAesGcmKey);

  crypto::Aead aead(crypto::Aead::AeadAlgorithm::AES_256_GCM);
  aead.Init(derived_key);

  std::vector<uint8_t> metadata_array(unencrypted_metadata_.ByteSizeLong());
  unencrypted_metadata_.SerializeToArray(metadata_array.data(),
                                         metadata_array.size());

  return aead.Seal(
      metadata_array,
      /*nonce=*/
      DeriveNearbyShareKey(base::as_bytes(base::make_span(secret_key_->key())),
                           kNearbyShareNumBytesAesGcmIv),
      /*additional_data=*/base::span<const uint8_t>());
}
