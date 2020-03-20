// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/v2_handshake.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/obj.h"

namespace device {
namespace cablev2 {

namespace {

class CableV2HandshakeTest : public ::testing::Test {
 public:
  CableV2HandshakeTest() {
    std::fill(psk_gen_key_.begin(), psk_gen_key_.end(), 0);
    std::fill(nonce_and_eid_.first.begin(), nonce_and_eid_.first.end(), 1);
    std::fill(nonce_and_eid_.second.begin(), nonce_and_eid_.second.end(), 2);
    std::fill(local_seed_.begin(), local_seed_.end(), 3);

    p256_key_.reset(EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
    const EC_GROUP* group = EC_KEY_get0_group(p256_key_.get());
    CHECK(EC_KEY_generate_key(p256_key_.get()));
    CHECK_EQ(p256_public_key_.size(),
             EC_POINT_point2oct(group, EC_KEY_get0_public_key(p256_key_.get()),
                                POINT_CONVERSION_UNCOMPRESSED,
                                p256_public_key_.data(),
                                p256_public_key_.size(), /*ctx=*/nullptr));
    bssl::UniquePtr<EC_KEY> qr_identity(EC_KEY_derive_from_secret(
        group, local_seed_.data(), local_seed_.size()));
    qr_identity_.reset(
        EC_POINT_dup(EC_KEY_get0_public_key(qr_identity.get()), group));
  }

 protected:
  std::array<uint8_t, 32> psk_gen_key_;
  NonceAndEID nonce_and_eid_;
  bssl::UniquePtr<EC_KEY> p256_key_;
  bssl::UniquePtr<EC_POINT> qr_identity_;
  std::array<uint8_t, kP256PointSize> p256_public_key_;
  std::array<uint8_t, kCableIdentityKeySeedSize> local_seed_;
};

TEST_F(CableV2HandshakeTest, MessageEncrytion) {
  std::array<uint8_t, 32> key1, key2;
  std::fill(key1.begin(), key1.end(), 1);
  std::fill(key2.begin(), key2.end(), 2);

  Crypter a(key1, key2);
  Crypter b(key2, key1);

  static constexpr FidoBleDeviceCommand command = FidoBleDeviceCommand::kMsg;
  static constexpr size_t kMaxSize = 530;
  std::vector<uint8_t> message, ciphertext, plaintext;
  message.reserve(kMaxSize);
  ciphertext.reserve(kMaxSize);
  plaintext.reserve(kMaxSize);

  for (size_t i = 0; i < kMaxSize; i++) {
    ciphertext = message;
    ASSERT_TRUE(a.Encrypt(&ciphertext));
    ASSERT_TRUE(b.Decrypt(command, ciphertext, &plaintext));
    ASSERT_TRUE(plaintext == message);

    ciphertext[(13 * i) % ciphertext.size()] ^= 1;
    ASSERT_FALSE(b.Decrypt(command, ciphertext, &plaintext));

    message.push_back(i & 0xff);
  }
}

TEST_F(CableV2HandshakeTest, OneTimeQRHandshake) {
  std::array<uint8_t, 32> wrong_psk_gen_key = psk_gen_key_;
  wrong_psk_gen_key[0] ^= 1;

  for (const bool use_correct_key : {false, true}) {
    HandshakeInitiator initiator(
        use_correct_key ? psk_gen_key_ : wrong_psk_gen_key,
        nonce_and_eid_.first, nonce_and_eid_.second,
        /*peer_identity=*/base::nullopt, local_seed_);
    std::vector<uint8_t> message = initiator.BuildInitialMessage();
    std::vector<uint8_t> response;
    base::Optional<std::unique_ptr<Crypter>> response_crypter(
        RespondToHandshake(psk_gen_key_, nonce_and_eid_, /*identity=*/nullptr,
                           qr_identity_.get(), /*pairing_data=*/nullptr,
                           message, &response));
    ASSERT_EQ(response_crypter.has_value(), use_correct_key);
    if (!use_correct_key) {
      continue;
    }

    base::Optional<
        std::pair<std::unique_ptr<Crypter>,
                  base::Optional<std::unique_ptr<CableDiscoveryData>>>>
        initiator_result(initiator.ProcessResponse(response));
    ASSERT_TRUE(initiator_result.has_value());
    EXPECT_FALSE(initiator_result->second.has_value());
    EXPECT_TRUE(response_crypter.value()->IsCounterpartyOfForTesting(
        *initiator_result->first));
  }
}

TEST_F(CableV2HandshakeTest, PairingQRHandshake) {
  CableDiscoveryData pairing;
  pairing.v2.emplace();
  std::fill(pairing.v2->eid_gen_key.begin(), pairing.v2->eid_gen_key.end(), 1);
  std::fill(pairing.v2->psk_gen_key.begin(), pairing.v2->psk_gen_key.end(), 2);
  pairing.v2->peer_identity = p256_public_key_;
  pairing.v2->peer_name = "Unittest";

  HandshakeInitiator initiator(psk_gen_key_, nonce_and_eid_.first,
                               nonce_and_eid_.second,
                               /*peer_identity=*/base::nullopt, local_seed_);
  std::vector<uint8_t> message = initiator.BuildInitialMessage();
  std::vector<uint8_t> response;
  base::Optional<std::unique_ptr<Crypter>> response_crypter(
      RespondToHandshake(psk_gen_key_, nonce_and_eid_, /*identity=*/nullptr,
                         qr_identity_.get(), &pairing, message, &response));
  ASSERT_TRUE(response_crypter.has_value());
  base::Optional<std::pair<std::unique_ptr<Crypter>,
                           base::Optional<std::unique_ptr<CableDiscoveryData>>>>
      initiator_result(initiator.ProcessResponse(response));
  ASSERT_TRUE(initiator_result.has_value());
  EXPECT_TRUE(initiator_result->second.has_value());
  EXPECT_EQ(initiator_result->second.value()->v2->eid_gen_key,
            pairing.v2->eid_gen_key);
  EXPECT_EQ(initiator_result->second.value()->v2->psk_gen_key,
            pairing.v2->psk_gen_key);
  EXPECT_EQ(initiator_result->second.value()->v2->peer_identity,
            pairing.v2->peer_identity);
  EXPECT_EQ(initiator_result->second.value()->v2->peer_name,
            pairing.v2->peer_name);
  EXPECT_TRUE(response_crypter.value()->IsCounterpartyOfForTesting(
      *initiator_result->first));
}

TEST_F(CableV2HandshakeTest, PairedHandshake) {
  bssl::UniquePtr<EC_KEY> wrong_key(
      EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
  CHECK(EC_KEY_generate_key(wrong_key.get()));

  for (const bool use_correct_key : {false, true}) {
    SCOPED_TRACE(use_correct_key);

    HandshakeInitiator initiator(psk_gen_key_, nonce_and_eid_.first,
                                 nonce_and_eid_.second, p256_public_key_,
                                 /*local_seed=*/base::nullopt);
    std::vector<uint8_t> message = initiator.BuildInitialMessage();
    std::vector<uint8_t> response;
    base::Optional<std::unique_ptr<Crypter>> response_crypter(
        RespondToHandshake(psk_gen_key_, nonce_and_eid_,
                           use_correct_key ? p256_key_.get() : wrong_key.get(),
                           /*peer_identity=*/nullptr,
                           /*pairing=*/nullptr, message, &response));
    ASSERT_EQ(response_crypter.has_value(), use_correct_key);

    if (!use_correct_key) {
      continue;
    }

    base::Optional<
        std::pair<std::unique_ptr<Crypter>,
                  base::Optional<std::unique_ptr<CableDiscoveryData>>>>
        initiator_result(initiator.ProcessResponse(response));
    ASSERT_TRUE(initiator_result.has_value());
    EXPECT_FALSE(initiator_result->second.has_value());
    EXPECT_TRUE(response_crypter.value()->IsCounterpartyOfForTesting(
        *initiator_result->first));
  }
}

}  // namespace
}  // namespace cablev2
}  // namespace device
