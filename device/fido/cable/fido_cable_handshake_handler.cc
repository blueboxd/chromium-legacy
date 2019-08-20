// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/fido_cable_handshake_handler.h"

#include <algorithm>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/containers/span.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "components/device_event_log/device_event_log.h"
#include "crypto/aead.h"
#include "crypto/hkdf.h"
#include "crypto/hmac.h"
#include "crypto/random.h"
#include "device/fido/cable/fido_cable_device.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/ecdh.h"
#include "third_party/boringssl/src/include/openssl/hkdf.h"
#include "third_party/boringssl/src/include/openssl/obj.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace device {

namespace {

// Length of CBOR encoded authenticator hello message concatenated with
// 16 byte message authentication code.
constexpr size_t kCableAuthenticatorHandshakeMessageSize = 66;

// Length of CBOR encoded client hello message concatenated with 16 byte message
// authenticator code.
constexpr size_t kClientHelloMessageSize = 58;

constexpr size_t kCableHandshakeMacMessageSize = 16;

// Derives key of size 32 bytes using HDKF algorithm.
// See https://tools.ietf.org/html/rfc5869 for details.
std::string GenerateKey(base::StringPiece secret,
                        base::StringPiece salt,
                        base::StringPiece info) {
  return crypto::HkdfSha256(secret, salt, info, 32);
}

base::Optional<std::array<uint8_t, kClientHelloMessageSize>>
ConstructHandshakeMessage(base::StringPiece handshake_key,
                          base::span<const uint8_t, 16> client_random_nonce) {
  cbor::Value::MapValue map;
  map.emplace(0, kCableClientHelloMessage);
  map.emplace(1, client_random_nonce);
  auto client_hello = cbor::Writer::Write(cbor::Value(std::move(map)));
  DCHECK(client_hello);

  crypto::HMAC hmac(crypto::HMAC::SHA256);
  if (!hmac.Init(handshake_key))
    return base::nullopt;

  std::array<uint8_t, 32> client_hello_mac;
  if (!hmac.Sign(fido_parsing_utils::ConvertToStringPiece(*client_hello),
                 client_hello_mac.data(), client_hello_mac.size())) {
    return base::nullopt;
  }

  DCHECK_EQ(kClientHelloMessageSize,
            client_hello->size() + kCableHandshakeMacMessageSize);
  std::array<uint8_t, kClientHelloMessageSize> handshake_message;
  std::copy(client_hello->begin(), client_hello->end(),
            handshake_message.begin());
  std::copy(client_hello_mac.begin(),
            client_hello_mac.begin() + kCableHandshakeMacMessageSize,
            handshake_message.begin() + client_hello->size());

  return handshake_message;
}

}  // namespace

FidoCableHandshakeHandler::~FidoCableHandshakeHandler() {}

FidoCableV1HandshakeHandler::FidoCableV1HandshakeHandler(
    FidoCableDevice* cable_device,
    base::span<const uint8_t, 8> nonce,
    base::span<const uint8_t, 32> session_pre_key)
    : cable_device_(cable_device),
      nonce_(fido_parsing_utils::Materialize(nonce)),
      session_pre_key_(fido_parsing_utils::Materialize(session_pre_key)),
      handshake_key_(GenerateKey(
          fido_parsing_utils::ConvertToStringPiece(session_pre_key_),
          fido_parsing_utils::ConvertToStringPiece(nonce_),
          kCableHandshakeKeyInfo)) {
  crypto::RandBytes(client_session_random_.data(),
                    client_session_random_.size());
}

FidoCableV1HandshakeHandler::~FidoCableV1HandshakeHandler() = default;

void FidoCableV1HandshakeHandler::InitiateCableHandshake(
    FidoDevice::DeviceCallback callback) {
  auto handshake_message =
      ConstructHandshakeMessage(handshake_key_, client_session_random_);
  if (!handshake_message) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
    return;
  }

  FIDO_LOG(DEBUG) << "Sending the caBLE handshake message";
  cable_device_->SendHandshakeMessage(
      fido_parsing_utils::Materialize(*handshake_message), std::move(callback));
}

bool FidoCableV1HandshakeHandler::ValidateAuthenticatorHandshakeMessage(
    base::span<const uint8_t> response) {
  crypto::HMAC hmac(crypto::HMAC::SHA256);
  if (!hmac.Init(handshake_key_))
    return false;

  if (response.size() != kCableAuthenticatorHandshakeMessageSize) {
    return false;
  }

  const auto authenticator_hello = response.first(
      kCableAuthenticatorHandshakeMessageSize - kCableHandshakeMacMessageSize);
  if (!hmac.VerifyTruncated(
          fido_parsing_utils::ConvertToStringPiece(authenticator_hello),
          fido_parsing_utils::ConvertToStringPiece(
              response.subspan(authenticator_hello.size())))) {
    return false;
  }

  const auto authenticator_hello_cbor = cbor::Reader::Read(authenticator_hello);
  if (!authenticator_hello_cbor || !authenticator_hello_cbor->is_map() ||
      authenticator_hello_cbor->GetMap().size() != 2) {
    return false;
  }

  const auto authenticator_hello_msg =
      authenticator_hello_cbor->GetMap().find(cbor::Value(0));
  if (authenticator_hello_msg == authenticator_hello_cbor->GetMap().end() ||
      !authenticator_hello_msg->second.is_string() ||
      authenticator_hello_msg->second.GetString() !=
          kCableAuthenticatorHelloMessage) {
    return false;
  }

  const auto authenticator_random_nonce =
      authenticator_hello_cbor->GetMap().find(cbor::Value(1));
  if (authenticator_random_nonce == authenticator_hello_cbor->GetMap().end() ||
      !authenticator_random_nonce->second.is_bytestring() ||
      authenticator_random_nonce->second.GetBytestring().size() != 16) {
    return false;
  }

  cable_device_->SetEncryptionData(
      GetEncryptionKeyAfterSuccessfulHandshake(base::make_span<16>(
          authenticator_random_nonce->second.GetBytestring())),
      nonce_);

  return true;
}

std::string
FidoCableV1HandshakeHandler::GetEncryptionKeyAfterSuccessfulHandshake(
    base::span<const uint8_t, 16> authenticator_random_nonce) const {
  std::vector<uint8_t> nonce_message;
  fido_parsing_utils::Append(&nonce_message, nonce_);
  fido_parsing_utils::Append(&nonce_message, client_session_random_);
  fido_parsing_utils::Append(&nonce_message, authenticator_random_nonce);
  return GenerateKey(
      fido_parsing_utils::ConvertToStringPiece(session_pre_key_),
      fido_parsing_utils::ConvertToStringPiece(
          fido_parsing_utils::CreateSHA256Hash(
              fido_parsing_utils::ConvertToStringPiece(nonce_message))),
      kCableDeviceEncryptionKeyInfo);
}

FidoCableV2HandshakeHandler::FidoCableV2HandshakeHandler(
    FidoCableDevice* cable_device,
    base::span<const uint8_t, 32> session_pre_key)
    : cable_device_(cable_device),
      session_pre_key_(fido_parsing_utils::Materialize(session_pre_key)) {}

FidoCableV2HandshakeHandler::~FidoCableV2HandshakeHandler() {}

namespace {

// P256PointSize is the number of bytes in an X9.62 encoding of a P-256 point.
constexpr size_t P256PointSize = 65;

// HKDF2 implements the functions with the same name from Noise[1], specialized
// to the case where |num_outputs| is two.
//
// [1] https://www.noiseprotocol.org/noise.html#hash-functions
std::tuple<std::array<uint8_t, 32>, std::array<uint8_t, 32>> HKDF2(
    base::span<const uint8_t, 32> ck,
    base::span<const uint8_t> ikm) {
  uint8_t output[32 * 2];
  HKDF(output, sizeof(output), EVP_sha256(), ikm.data(), ikm.size(), ck.data(),
       ck.size(), /*salt=*/nullptr, 0);

  std::array<uint8_t, 32> a, b;
  memcpy(a.data(), &output[0], 32);
  memcpy(b.data(), &output[32], 32);

  return std::make_tuple(a, b);
}

// HKDF3 implements the functions with the same name from Noise[1], specialized
// to the case where |num_outputs| is three.
//
// [1] https://www.noiseprotocol.org/noise.html#hash-functions
std::tuple<std::array<uint8_t, 32>,
           std::array<uint8_t, 32>,
           std::array<uint8_t, 32>>
HKDF3(base::span<const uint8_t, 32> ck, base::span<const uint8_t> ikm) {
  uint8_t output[32 * 3];
  HKDF(output, sizeof(output), EVP_sha256(), ikm.data(), ikm.size(), ck.data(),
       ck.size(), /*salt=*/nullptr, 0);

  std::array<uint8_t, 32> a, b, c;
  memcpy(a.data(), &output[0], 32);
  memcpy(b.data(), &output[32], 32);
  memcpy(c.data(), &output[64], 32);

  return std::make_tuple(a, b, c);
}

}  // namespace

void FidoCableV2HandshakeHandler::InitiateCableHandshake(
    FidoDevice::DeviceCallback callback) {
  // See https://www.noiseprotocol.org/noise.html#the-handshakestate-object
  static const char kProtocolName[] = "Noise_NNpsk0_P256_AESGCM_SHA256";
  static_assert(sizeof(kProtocolName) == crypto::kSHA256Length,
                "name may need padding if not HASHLEN bytes long");
  static_assert(
      std::tuple_size<decltype(chaining_key_)>::value == crypto::kSHA256Length,
      "chaining_key_ is wrong size");
  static_assert(std::tuple_size<decltype(h_)>::value == crypto::kSHA256Length,
                "h_ is wrong size");
  memcpy(chaining_key_.data(), kProtocolName, sizeof(kProtocolName));
  h_ = chaining_key_;

  static const uint8_t kPrologue[] = "caBLE QR code handshake";
  MixHash(kPrologue);

  MixKeyAndHash(session_pre_key_);
  ephemeral_key_.reset(EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
  CHECK(EC_KEY_generate_key(ephemeral_key_.get()));
  uint8_t ephemeral_key_public_bytes[P256PointSize];
  CHECK_EQ(sizeof(ephemeral_key_public_bytes),
           EC_POINT_point2oct(
               EC_KEY_get0_group(ephemeral_key_.get()),
               EC_KEY_get0_public_key(ephemeral_key_.get()),
               POINT_CONVERSION_UNCOMPRESSED, ephemeral_key_public_bytes,
               sizeof(ephemeral_key_public_bytes), /*ctx=*/nullptr));
  MixHash(ephemeral_key_public_bytes);
  MixKey(ephemeral_key_public_bytes);

  std::vector<uint8_t> ciphertext = Encrypt(base::span<const uint8_t>());
  MixHash(ciphertext);

  std::vector<uint8_t> handshake_message;
  handshake_message.reserve(sizeof(ephemeral_key_public_bytes) +
                            ciphertext.size());
  handshake_message.insert(
      handshake_message.end(), ephemeral_key_public_bytes,
      ephemeral_key_public_bytes + sizeof(ephemeral_key_public_bytes));
  handshake_message.insert(handshake_message.end(), ciphertext.begin(),
                           ciphertext.end());

  cable_device_->SendHandshakeMessage(std::move(handshake_message),
                                      std::move(callback));
}

bool FidoCableV2HandshakeHandler::ValidateAuthenticatorHandshakeMessage(
    base::span<const uint8_t> response) {
  if (response.size() < P256PointSize) {
    return false;
  }
  auto peer_point_bytes = response.subspan(0, P256PointSize);
  auto ciphertext = response.subspan(P256PointSize);

  bssl::UniquePtr<EC_POINT> peer_point(
      EC_POINT_new(EC_KEY_get0_group(ephemeral_key_.get())));
  uint8_t shared_key[32];
  if (!EC_POINT_oct2point(EC_KEY_get0_group(ephemeral_key_.get()),
                          peer_point.get(), peer_point_bytes.data(),
                          peer_point_bytes.size(), /*ctx=*/nullptr) ||
      !ECDH_compute_key(shared_key, sizeof(shared_key), peer_point.get(),
                        ephemeral_key_.get(), /*kdf=*/nullptr)) {
    return false;
  }

  MixHash(peer_point_bytes);
  MixKey(peer_point_bytes);
  MixKey(shared_key);

  auto maybe_plaintext = Decrypt(ciphertext);
  if (!maybe_plaintext || !maybe_plaintext->empty()) {
    return false;
  }
  // Here the spec says to do MixHash(ciphertext), but there are no more
  // handshake messages so that's moot.
  // MixHash(ciphertext);

  std::array<uint8_t, 32> key1, unused_key2;
  std::tie(key1, unused_key2) =
      HKDF2(chaining_key_, base::span<const uint8_t>());

  uint8_t zero_nonce[8] = {0};
  cable_device_->SetEncryptionData(
      std::string(reinterpret_cast<const char*>(key1.data()), key1.size()),
      zero_nonce);

  return true;
}

void FidoCableV2HandshakeHandler::MixHash(base::span<const uint8_t> in) {
  // See https://www.noiseprotocol.org/noise.html#the-symmetricstate-object
  SHA256_CTX ctx;
  SHA256_Init(&ctx);
  SHA256_Update(&ctx, h_.data(), h_.size());
  SHA256_Update(&ctx, in.data(), in.size());
  SHA256_Final(h_.data(), &ctx);
}

void FidoCableV2HandshakeHandler::MixKey(base::span<const uint8_t> ikm) {
  // See https://www.noiseprotocol.org/noise.html#the-symmetricstate-object
  std::array<uint8_t, 32> temp_k;
  std::tie(chaining_key_, temp_k) = HKDF2(chaining_key_, ikm);
  InitializeKey(temp_k);
}

void FidoCableV2HandshakeHandler::MixKeyAndHash(base::span<const uint8_t> ikm) {
  // See https://www.noiseprotocol.org/noise.html#the-symmetricstate-object
  std::array<uint8_t, 32> temp_h, temp_k;
  std::tie(chaining_key_, temp_h, temp_k) = HKDF3(chaining_key_, ikm);
  MixHash(temp_h);
  InitializeKey(temp_k);
}

void FidoCableV2HandshakeHandler::InitializeKey(
    base::span<const uint8_t, 32> key) {
  // See https://www.noiseprotocol.org/noise.html#the-cipherstate-object
  DCHECK_EQ(symmetric_key_.size(), key.size());
  memcpy(symmetric_key_.data(), key.data(), symmetric_key_.size());
  symmetric_nonce_ = 0;
}

std::vector<uint8_t> FidoCableV2HandshakeHandler::Encrypt(
    base::span<const uint8_t> plaintext) {
  uint8_t nonce[12] = {0};
  nonce[8] = symmetric_nonce_ >> 24;
  nonce[9] = symmetric_nonce_ >> 16;
  nonce[10] = symmetric_nonce_ >> 8;
  nonce[11] = symmetric_nonce_;
  symmetric_nonce_++;

  crypto::Aead aead(crypto::Aead::AES_256_GCM);
  aead.Init(symmetric_key_);
  return aead.Seal(base::span<const uint8_t>(), nonce, h_);
}

base::Optional<std::vector<uint8_t>> FidoCableV2HandshakeHandler::Decrypt(
    base::span<const uint8_t> ciphertext) {
  uint8_t nonce[12] = {0};
  nonce[8] = symmetric_nonce_ >> 24;
  nonce[9] = symmetric_nonce_ >> 16;
  nonce[10] = symmetric_nonce_ >> 8;
  nonce[11] = symmetric_nonce_;
  symmetric_nonce_++;

  crypto::Aead aead(crypto::Aead::AES_256_GCM);
  aead.Init(symmetric_key_);
  return aead.Open(ciphertext, nonce, h_);
}

}  // namespace device
