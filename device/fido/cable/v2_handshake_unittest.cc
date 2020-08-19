// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/v2_handshake.h"
#include "base/rand_util.h"
#include "components/cbor/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/obj.h"
#include "url/gurl.h"

namespace device {
namespace cablev2 {

namespace {

TEST(CableV2Encoding, TunnelServerURLs) {
  // Test that a domain name survives an encode-decode round trip.
  constexpr uint32_t encoded =
      tunnelserver::EncodeDomain("abcd", tunnelserver::TLD::NET);
  uint8_t tunnel_id[16] = {0};
  const GURL url =
      tunnelserver::GetURL(encoded, tunnelserver::Action::kNew, tunnel_id);
  EXPECT_TRUE(url.spec().find("//abcd.net/") != std::string::npos) << url;
}

TEST(CableV2Encoding, EIDs) {
  eid::Components components;
  components.tunnel_server_domain = 0x010203;
  components.shard_id = 42;
  base::RandBytes(components.nonce.data(), components.nonce.size());

  CableEidArray eid = eid::FromComponents(components);
  eid::Components components2 = eid::ToComponents(eid);

  EXPECT_EQ(components.shard_id, components2.shard_id);
  EXPECT_EQ(components.tunnel_server_domain, components2.tunnel_server_domain);
  EXPECT_EQ(components.nonce, components2.nonce);

  for (size_t i = 0; i < eid.size(); i++) {
    eid[i] ^= 0xff;
  }

  EXPECT_FALSE(eid::IsValid(eid));
}

TEST(CableV2Encoding, PaddedCBOR) {
  cbor::Value::MapValue map;
  base::Optional<std::vector<uint8_t>> encoded =
      EncodePaddedCBORMap(std::move(map));
  ASSERT_TRUE(encoded);
  EXPECT_EQ(256u, encoded->size());

  base::Optional<cbor::Value> decoded = DecodePaddedCBORMap(*encoded);
  ASSERT_TRUE(decoded);
  EXPECT_EQ(0u, decoded->GetMap().size());

  uint8_t blob[256] = {0};
  map.emplace(1, base::span<const uint8_t>(blob, sizeof(blob)));
  encoded = EncodePaddedCBORMap(std::move(map));
  ASSERT_TRUE(encoded);
  EXPECT_EQ(512u, encoded->size());

  decoded = DecodePaddedCBORMap(*encoded);
  ASSERT_TRUE(decoded);
  EXPECT_EQ(1u, decoded->GetMap().size());
}

class CableV2HandshakeTest : public ::testing::Test {
 public:
  CableV2HandshakeTest() {
    std::fill(psk_gen_key_.begin(), psk_gen_key_.end(), 0);
    std::fill(nonce_and_eid_.first.begin(), nonce_and_eid_.first.end(), 1);
    std::fill(nonce_and_eid_.second.begin(), nonce_and_eid_.second.end(), 2);
    std::fill(identity_seed_.begin(), identity_seed_.end(), 3);

    bssl::UniquePtr<EC_GROUP> group(
        EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
    identity_key_.reset(EC_KEY_derive_from_secret(
        group.get(), identity_seed_.data(), identity_seed_.size()));
    CHECK_EQ(identity_public_.size(),
             EC_POINT_point2oct(
                 group.get(), EC_KEY_get0_public_key(identity_key_.get()),
                 POINT_CONVERSION_UNCOMPRESSED, identity_public_.data(),
                 identity_public_.size(), /*ctx=*/nullptr));
  }

 protected:
  std::array<uint8_t, 32> psk_gen_key_;
  NonceAndEID nonce_and_eid_;
  bssl::UniquePtr<EC_KEY> identity_key_;
  std::array<uint8_t, kP256X962Length> identity_public_;
  std::array<uint8_t, kCableIdentityKeySeedSize> identity_seed_;
};

TEST_F(CableV2HandshakeTest, MessageEncrytion) {
  std::array<uint8_t, 32> key1, key2;
  std::fill(key1.begin(), key1.end(), 1);
  std::fill(key2.begin(), key2.end(), 2);

  Crypter a(key1, key2);
  Crypter b(key2, key1);

  static constexpr size_t kMaxSize = 530;
  std::vector<uint8_t> message, ciphertext, plaintext;
  message.reserve(kMaxSize);
  ciphertext.reserve(kMaxSize);
  plaintext.reserve(kMaxSize);

  for (size_t i = 0; i < kMaxSize; i++) {
    ciphertext = message;
    ASSERT_TRUE(a.Encrypt(&ciphertext));
    ASSERT_TRUE(b.Decrypt(ciphertext, &plaintext));
    ASSERT_TRUE(plaintext == message);

    ciphertext[(13 * i) % ciphertext.size()] ^= 1;
    ASSERT_FALSE(b.Decrypt(ciphertext, &plaintext));

    message.push_back(i & 0xff);
  }
}

TEST_F(CableV2HandshakeTest, QRHandshake) {
  std::array<uint8_t, 32> wrong_psk_gen_key = psk_gen_key_;
  wrong_psk_gen_key[0] ^= 1;
  uint8_t kGetInfoBytes[] = {1, 2, 3, 4, 5};

  for (const bool use_correct_key : {false, true}) {
    HandshakeInitiator initiator(
        use_correct_key ? psk_gen_key_ : wrong_psk_gen_key,
        nonce_and_eid_.first, identity_public_,
        /*local_identity=*/nullptr);
    std::vector<uint8_t> message =
        initiator.BuildInitialMessage(nonce_and_eid_.second, kGetInfoBytes);
    std::vector<uint8_t> response;
    base::Optional<std::pair<std::unique_ptr<Crypter>, std::vector<uint8_t>>>
        responder_result(RespondToHandshake(
            psk_gen_key_, nonce_and_eid_, identity_seed_,
            /*peer_identity=*/base::nullopt, message, &response));
    ASSERT_EQ(responder_result.has_value(), use_correct_key);
    if (!use_correct_key) {
      continue;
    }

    base::Optional<std::unique_ptr<Crypter>> initiator_result(
        initiator.ProcessResponse(response));
    ASSERT_TRUE(initiator_result.has_value());
    EXPECT_TRUE(responder_result->first->IsCounterpartyOfForTesting(
        *initiator_result.value()));
    ASSERT_EQ(responder_result->second.size(), sizeof(kGetInfoBytes));
    EXPECT_EQ(0, memcmp(responder_result->second.data(), kGetInfoBytes,
                        sizeof(kGetInfoBytes)));
  }
}

TEST_F(CableV2HandshakeTest, PairedHandshake) {
  bssl::UniquePtr<EC_KEY> wrong_key(
      EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
  CHECK(EC_KEY_generate_key(wrong_key.get()));
  uint8_t kGetInfoBytes[] = {1, 2, 3, 4, 5};

  for (const bool use_correct_key : {false, true}) {
    SCOPED_TRACE(use_correct_key);

    EC_KEY* const key = use_correct_key ? identity_key_.get() : wrong_key.get();
    EC_KEY_up_ref(key);
    HandshakeInitiator initiator(psk_gen_key_, nonce_and_eid_.first,
                                 /*peer_identity=*/base::nullopt,
                                 bssl::UniquePtr<EC_KEY>(key));
    std::vector<uint8_t> message =
        initiator.BuildInitialMessage(nonce_and_eid_.second, kGetInfoBytes);
    std::vector<uint8_t> response;
    base::Optional<std::pair<std::unique_ptr<Crypter>, std::vector<uint8_t>>>
        responder_result(RespondToHandshake(psk_gen_key_, nonce_and_eid_,
                                            /*identity_seed=*/base::nullopt,
                                            identity_public_, message,
                                            &response));
    ASSERT_EQ(responder_result.has_value(), use_correct_key);

    if (!use_correct_key) {
      continue;
    }

    base::Optional<std::unique_ptr<Crypter>> initiator_result(
        initiator.ProcessResponse(response));
    ASSERT_TRUE(initiator_result.has_value());
    EXPECT_TRUE(responder_result->first->IsCounterpartyOfForTesting(
        *initiator_result.value()));
    ASSERT_EQ(responder_result->second.size(), sizeof(kGetInfoBytes));
    EXPECT_EQ(0, memcmp(responder_result->second.data(), kGetInfoBytes,
                        sizeof(kGetInfoBytes)));
  }
}

}  // namespace
}  // namespace cablev2
}  // namespace device
