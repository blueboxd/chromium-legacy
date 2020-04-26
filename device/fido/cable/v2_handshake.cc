// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/v2_handshake.h"

#include "base/numerics/safe_math.h"
#include "base/strings/string_number_conversions.h"  // TODO REMOVE
#include "components/cbor/reader.h"
#include "components/cbor/writer.h"
#include "components/device_event_log/device_event_log.h"
#include "crypto/aead.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/ecdh.h"
#include "third_party/boringssl/src/include/openssl/hkdf.h"
#include "third_party/boringssl/src/include/openssl/obj.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

using device::fido_parsing_utils::CopyCBORBytestring;

namespace {

// Maximum value of a sequence number. Exceeding this causes all operations to
// return an error. This is assumed to be vastly larger than any caBLE exchange
// will ever reach.
constexpr uint32_t kMaxSequence = (1 << 24) - 1;

bool ConstructNonce(uint32_t counter, base::span<uint8_t, 12> out_nonce) {
  if (counter > kMaxSequence) {
    return false;
  }

  // Nonce is just a little-endian counter.
  std::array<uint8_t, sizeof(counter)> counter_bytes;
  memcpy(counter_bytes.data(), &counter, sizeof(counter));
  std::copy(counter_bytes.begin(), counter_bytes.end(), out_nonce.begin());
  std::fill(out_nonce.begin() + counter_bytes.size(), out_nonce.end(), 0);
  return true;
}

CBS CBSFromSpan(base::span<const uint8_t> in) {
  CBS cbs;
  CBS_init(&cbs, in.data(), in.size());
  return cbs;
}

bool CBS_get_span(CBS* cbs, base::span<const uint8_t>* out_span, size_t N) {
  CBS contents;
  if (!CBS_get_bytes(cbs, &contents, N)) {
    return false;
  }
  *out_span =
      base::span<const uint8_t>(CBS_data(&contents), CBS_len(&contents));
  return true;
}

constexpr uint8_t kPairedPrologue[] = "caBLE handshake";
constexpr uint8_t kQRPrologue[] = "caBLE QR code handshake";

}  // namespace

namespace device {
namespace cablev2 {

Crypter::Crypter(base::span<const uint8_t, 32> read_key,
                 base::span<const uint8_t, 32> write_key)
    : read_key_(fido_parsing_utils::Materialize(read_key)),
      write_key_(fido_parsing_utils::Materialize(write_key)) {}

Crypter::~Crypter() = default;

bool Crypter::Encrypt(std::vector<uint8_t>* message_to_encrypt) {
  // Messages will be padded in order to round their length up to a multiple
  // of kPaddingGranularity.
  constexpr size_t kPaddingGranularity = 32;
  static_assert(kPaddingGranularity > 0, "padding too small");
  static_assert(kPaddingGranularity < 256, "padding too large");
  static_assert((kPaddingGranularity & (kPaddingGranularity - 1)) == 0,
                "padding must be a power of two");

  // Padding consists of a some number of zero bytes appended to the message
  // and the final byte in the message is the number of zeros.
  base::CheckedNumeric<size_t> padded_size_checked = message_to_encrypt->size();
  padded_size_checked += 1;  // padding-length byte.
  padded_size_checked = (padded_size_checked + kPaddingGranularity - 1) &
                        ~(kPaddingGranularity - 1);
  if (!padded_size_checked.IsValid()) {
    NOTREACHED();
    return false;
  }

  const size_t padded_size = padded_size_checked.ValueOrDie();
  CHECK_GT(padded_size, message_to_encrypt->size());
  const size_t num_zeros = padded_size - message_to_encrypt->size() - 1;

  std::vector<uint8_t> padded_message(padded_size, 0);
  memcpy(padded_message.data(), message_to_encrypt->data(),
         message_to_encrypt->size());
  // The number of added zeros has to fit in a single byte so it has to be
  // less than 256.
  DCHECK_LT(num_zeros, 256u);
  padded_message[padded_message.size() - 1] = static_cast<uint8_t>(num_zeros);

  std::array<uint8_t, 12> nonce;
  if (!ConstructNonce(write_sequence_num_++, nonce)) {
    return false;
  }

  crypto::Aead aes_key(crypto::Aead::AES_256_GCM);
  aes_key.Init(write_key_);
  DCHECK_EQ(nonce.size(), aes_key.NonceLength());

  const uint8_t additional_data[2] = {
      base::strict_cast<uint8_t>(device::FidoBleDeviceCommand::kMsg),
      /*version=*/2};
  std::vector<uint8_t> ciphertext =
      aes_key.Seal(padded_message, nonce, additional_data);
  message_to_encrypt->swap(ciphertext);
  return true;
}

bool Crypter::Decrypt(FidoBleDeviceCommand command,
                      base::span<const uint8_t> ciphertext,
                      std::vector<uint8_t>* out_plaintext) {
  std::array<uint8_t, 12> nonce;
  if (!ConstructNonce(read_sequence_num_, nonce)) {
    return false;
  }

  crypto::Aead aes_key(crypto::Aead::AES_256_GCM);
  aes_key.Init(read_key_);
  DCHECK_EQ(nonce.size(), aes_key.NonceLength());

  const uint8_t additional_data[2] = {base::strict_cast<uint8_t>(command),
                                      /*version=*/2};
  base::Optional<std::vector<uint8_t>> plaintext =
      aes_key.Open(ciphertext, nonce, additional_data);

  if (!plaintext) {
    return false;
  }
  read_sequence_num_++;

  if (plaintext->empty()) {
    FIDO_LOG(ERROR) << "Invalid caBLE message.";
    return false;
  }

  const size_t padding_length = (*plaintext)[plaintext->size() - 1];
  if (padding_length + 1 > plaintext->size()) {
    FIDO_LOG(ERROR) << "Invalid caBLE message.";
    return false;
  }
  plaintext->resize(plaintext->size() - padding_length - 1);

  out_plaintext->swap(*plaintext);
  return true;
}

bool Crypter::IsCounterpartyOfForTesting(const Crypter& other) const {
  return read_key_ == other.write_key_ && write_key_ == other.read_key_;
}

HandshakeInitiator::HandshakeInitiator(
    base::span<const uint8_t, 32> psk_gen_key,
    base::span<const uint8_t, 8> nonce,
    base::span<const uint8_t, kCableEphemeralIdSize> eid,
    base::Optional<base::span<const uint8_t, kP256PointSize>> peer_identity,
    base::Optional<base::span<const uint8_t, kCableIdentityKeySeedSize>>
        local_seed)
    : eid_(fido_parsing_utils::Materialize(eid)) {
  DCHECK(peer_identity.has_value() ^ local_seed.has_value());
  HKDF(psk_.data(), psk_.size(), EVP_sha256(), psk_gen_key.data(),
       psk_gen_key.size(), /*salt=*/nonce.data(), nonce.size(),
       /*info=*/nullptr, 0);
  if (peer_identity) {
    peer_identity_ = fido_parsing_utils::Materialize(*peer_identity);
  }
  if (local_seed) {
    local_seed_ = fido_parsing_utils::Materialize(*local_seed);
  }
}

HandshakeInitiator::~HandshakeInitiator() = default;

std::vector<uint8_t> HandshakeInitiator::BuildInitialMessage() {
  if (peer_identity_) {
    noise_.Init(Noise::HandshakeType::kNKpsk0);
    noise_.MixHash(kPairedPrologue);
  } else {
    noise_.Init(Noise::HandshakeType::kKNpsk0);
    noise_.MixHash(kQRPrologue);
  }

  noise_.MixKeyAndHash(psk_);
  ephemeral_key_.reset(EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
  const EC_GROUP* group = EC_KEY_get0_group(ephemeral_key_.get());
  CHECK(EC_KEY_generate_key(ephemeral_key_.get()));
  uint8_t ephemeral_key_public_bytes[kP256PointSize];
  CHECK_EQ(sizeof(ephemeral_key_public_bytes),
           EC_POINT_point2oct(
               group, EC_KEY_get0_public_key(ephemeral_key_.get()),
               POINT_CONVERSION_UNCOMPRESSED, ephemeral_key_public_bytes,
               sizeof(ephemeral_key_public_bytes), /*ctx=*/nullptr));
  noise_.MixHash(ephemeral_key_public_bytes);
  noise_.MixKey(ephemeral_key_public_bytes);

  if (peer_identity_) {
    // If we know the identity of the peer from a previous interaction, NKpsk0
    // is performed to ensure that other browsers, which may also know the PSK,
    // cannot impersonate the authenticator.
    bssl::UniquePtr<EC_POINT> peer_identity_point(EC_POINT_new(group));
    uint8_t es_key[32];
    CHECK(EC_POINT_oct2point(group, peer_identity_point.get(),
                             peer_identity_->data(), peer_identity_->size(),
                             /*ctx=*/nullptr));
    CHECK(ECDH_compute_key(es_key, sizeof(es_key), peer_identity_point.get(),
                           ephemeral_key_.get(),
                           /*kdf=*/nullptr) == sizeof(es_key));
    noise_.MixKey(es_key);
  }

  std::vector<uint8_t> ciphertext =
      noise_.EncryptAndHash(base::span<const uint8_t>());

  std::vector<uint8_t> handshake_message;
  handshake_message.reserve(eid_.size() + sizeof(ephemeral_key_public_bytes) +
                            ciphertext.size());
  handshake_message.insert(handshake_message.end(), eid_.begin(), eid_.end());
  handshake_message.insert(
      handshake_message.end(), ephemeral_key_public_bytes,
      ephemeral_key_public_bytes + sizeof(ephemeral_key_public_bytes));
  handshake_message.insert(handshake_message.end(), ciphertext.begin(),
                           ciphertext.end());

  return handshake_message;
}

base::Optional<std::pair<std::unique_ptr<Crypter>,
                         base::Optional<std::unique_ptr<CableDiscoveryData>>>>
HandshakeInitiator::ProcessResponse(base::span<const uint8_t> response) {
  if (response.size() < kP256PointSize) {
    return base::nullopt;
  }
  auto peer_point_bytes = response.subspan(0, kP256PointSize);
  auto ciphertext = response.subspan(kP256PointSize);

  bssl::UniquePtr<EC_POINT> peer_point(
      EC_POINT_new(EC_KEY_get0_group(ephemeral_key_.get())));
  uint8_t shared_key_ee[32];
  const EC_GROUP* group = EC_KEY_get0_group(ephemeral_key_.get());
  if (!EC_POINT_oct2point(group, peer_point.get(), peer_point_bytes.data(),
                          peer_point_bytes.size(), /*ctx=*/nullptr) ||
      ECDH_compute_key(shared_key_ee, sizeof(shared_key_ee), peer_point.get(),
                       ephemeral_key_.get(),
                       /*kdf=*/nullptr) != sizeof(shared_key_ee)) {
    FIDO_LOG(DEBUG) << "Peer's P-256 point not on curve.";
    return base::nullopt;
  }

  noise_.MixHash(peer_point_bytes);
  noise_.MixKey(peer_point_bytes);
  noise_.MixKey(shared_key_ee);

  if (local_seed_) {
    uint8_t shared_key_se[32];
    bssl::UniquePtr<EC_KEY> identity_key(EC_KEY_derive_from_secret(
        group, local_seed_->data(), local_seed_->size()));
    if (ECDH_compute_key(shared_key_se, sizeof(shared_key_se), peer_point.get(),
                         identity_key.get(),
                         /*kdf=*/nullptr) != sizeof(shared_key_se)) {
      return base::nullopt;
    }
    noise_.MixKey(shared_key_se);
  }

  auto plaintext = noise_.DecryptAndHash(ciphertext);
  if (!plaintext || plaintext->empty() != peer_identity_.has_value()) {
    FIDO_LOG(DEBUG) << "Invalid caBLE handshake message";
    return base::nullopt;
  }

  base::Optional<std::unique_ptr<CableDiscoveryData>> discovery_data;
  if (!peer_identity_) {
    // Handshakes without a peer identity (i.e. KNpsk0 handshakes setup from a
    // QR code) send a padded message in the reply. This message can,
    // optionally, contain CBOR-encoded, long-term pairing information.
    const size_t padding_length = (*plaintext)[plaintext->size() - 1];
    if (padding_length + 1 > plaintext->size()) {
      FIDO_LOG(DEBUG) << "Invalid padding in caBLE handshake message";
      return base::nullopt;
    }
    plaintext->resize(plaintext->size() - padding_length - 1);

    if (!plaintext->empty()) {
      base::Optional<cbor::Value> pairing = cbor::Reader::Read(*plaintext);
      if (!pairing || !pairing->is_map()) {
        FIDO_LOG(DEBUG) << "CBOR parse failure in caBLE handshake message";
        return base::nullopt;
      }

      auto future_discovery = std::make_unique<CableDiscoveryData>();
      future_discovery->version = CableDiscoveryData::Version::V2;
      future_discovery->v2.emplace();
      future_discovery->v2->peer_identity.emplace();

      const cbor::Value::MapValue& pairing_map(pairing->GetMap());
      const auto name_it = pairing_map.find(cbor::Value(4));
      if (!CopyCBORBytestring(&future_discovery->v2->eid_gen_key, pairing_map,
                              1) ||
          !CopyCBORBytestring(&future_discovery->v2->psk_gen_key, pairing_map,
                              2) ||
          !CopyCBORBytestring(&future_discovery->v2->peer_identity.value(),
                              pairing_map, 3) ||
          name_it == pairing_map.end() || !name_it->second.is_string() ||
          !EC_POINT_oct2point(group, peer_point.get(),
                              future_discovery->v2->peer_identity->data(),
                              future_discovery->v2->peer_identity->size(),
                              /*ctx=*/nullptr)) {
        FIDO_LOG(DEBUG) << "CBOR structure error in caBLE handshake message";
        return base::nullopt;
      }

      future_discovery->v2->peer_name = name_it->second.GetString();
      discovery_data.emplace(std::move(future_discovery));
    }
  }

  std::array<uint8_t, 32> read_key, write_key;
  std::tie(write_key, read_key) = noise_.traffic_keys();
  return std::make_pair(std::make_unique<cablev2::Crypter>(read_key, write_key),
                        std::move(discovery_data));
}

base::Optional<std::unique_ptr<Crypter>> RespondToHandshake(
    base::span<const uint8_t, 32> psk_gen_key,
    const NonceAndEID& nonce_and_eid,
    const EC_KEY* identity,
    const EC_POINT* peer_identity,
    const CableDiscoveryData* pairing_data,
    base::span<const uint8_t> in,
    std::vector<uint8_t>* out_response) {
  DCHECK(identity == nullptr || pairing_data == nullptr);
  DCHECK(identity == nullptr ^ peer_identity == nullptr);

  CBS cbs = CBSFromSpan(in);

  base::span<const uint8_t> eid;
  base::span<const uint8_t> peer_point_bytes;
  base::span<const uint8_t> ciphertext;
  if (!CBS_get_span(&cbs, &eid, device::kCableEphemeralIdSize) ||
      !CBS_get_span(&cbs, &peer_point_bytes, kP256PointSize) ||
      !CBS_get_span(&cbs, &ciphertext, 16) || CBS_len(&cbs) != 0) {
    return base::nullopt;
  }

  if (eid.size() != nonce_and_eid.second.size() ||
      memcmp(eid.data(), nonce_and_eid.second.data(), eid.size()) != 0) {
    return base::nullopt;
  }

  Noise noise;
  if (identity) {
    noise.Init(device::Noise::HandshakeType::kNKpsk0);
    noise.MixHash(kPairedPrologue);
  } else {
    noise.Init(device::Noise::HandshakeType::kKNpsk0);
    noise.MixHash(kQRPrologue);
  }

  std::array<uint8_t, 32> psk;
  HKDF(psk.data(), psk.size(), EVP_sha256(), psk_gen_key.data(),
       psk_gen_key.size(),
       /*salt=*/nonce_and_eid.first.data(), nonce_and_eid.first.size(),
       /*info=*/nullptr, 0);

  noise.MixKeyAndHash(psk);
  noise.MixHash(peer_point_bytes);
  noise.MixKey(peer_point_bytes);

  bssl::UniquePtr<EC_KEY> ephemeral_key(
      EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
  const EC_GROUP* group = EC_KEY_get0_group(ephemeral_key.get());
  CHECK(EC_KEY_generate_key(ephemeral_key.get()));
  bssl::UniquePtr<EC_POINT> peer_point(EC_POINT_new(group));
  if (!EC_POINT_oct2point(group, peer_point.get(), peer_point_bytes.data(),
                          peer_point_bytes.size(),
                          /*ctx=*/nullptr)) {
    FIDO_LOG(DEBUG) << "Peer's P-256 point not on curve.";
    return base::nullopt;
  }

  if (identity) {
    uint8_t es_key[32];
    if (ECDH_compute_key(es_key, sizeof(es_key), peer_point.get(), identity,
                         /*kdf=*/nullptr) != sizeof(es_key)) {
      return base::nullopt;
    }
    noise.MixKey(es_key);
  }

  auto plaintext = noise.DecryptAndHash(ciphertext);
  if (!plaintext || !plaintext->empty()) {
    FIDO_LOG(DEBUG) << "Failed to decrypt handshake ciphertext.";
    return base::nullopt;
  }

  uint8_t ephemeral_key_public_bytes[kP256PointSize];
  CHECK_EQ(sizeof(ephemeral_key_public_bytes),
           EC_POINT_point2oct(
               group, EC_KEY_get0_public_key(ephemeral_key.get()),
               POINT_CONVERSION_UNCOMPRESSED, ephemeral_key_public_bytes,
               sizeof(ephemeral_key_public_bytes), /*ctx=*/nullptr));
  noise.MixHash(ephemeral_key_public_bytes);
  noise.MixKey(ephemeral_key_public_bytes);

  uint8_t shared_key_ee[32];
  if (ECDH_compute_key(shared_key_ee, sizeof(shared_key_ee), peer_point.get(),
                       ephemeral_key.get(),
                       /*kdf=*/nullptr) != sizeof(shared_key_ee)) {
    return base::nullopt;
  }
  noise.MixKey(shared_key_ee);

  if (peer_identity) {
    uint8_t shared_key_se[32];
    if (ECDH_compute_key(shared_key_se, sizeof(shared_key_se), peer_identity,
                         ephemeral_key.get(),
                         /*kdf=*/nullptr) != sizeof(shared_key_se)) {
      return base::nullopt;
    }
    noise.MixKey(shared_key_se);
  }

  std::vector<uint8_t> my_ciphertext;
  if (!identity) {
    uint8_t my_plaintext[256];
    memset(my_plaintext, 0, sizeof(my_plaintext));

    if (pairing_data) {
      cbor::Value::MapValue pairing;
      pairing.emplace(1, pairing_data->v2->eid_gen_key);
      pairing.emplace(2, pairing_data->v2->psk_gen_key);
      pairing.emplace(3, pairing_data->v2->peer_identity.value());
      pairing.emplace(4, pairing_data->v2->peer_name.value());
      base::Optional<std::vector<uint8_t>> cbor_bytes =
          cbor::Writer::Write(cbor::Value(std::move(pairing)));
      if (!cbor_bytes || cbor_bytes->size() > sizeof(my_plaintext) - 1) {
        FIDO_LOG(DEBUG) << "Pairing encoding failed or result too large.";
        return base::nullopt;
      }
      memcpy(my_plaintext, cbor_bytes->data(), cbor_bytes->size());
      my_plaintext[255] = sizeof(my_plaintext) - 1 - cbor_bytes->size();
    } else {
      my_plaintext[255] = 255;
    }
    my_ciphertext = noise.EncryptAndHash(my_plaintext);
  } else {
    my_ciphertext = noise.EncryptAndHash(base::span<const uint8_t>());
  }

  out_response->insert(
      out_response->end(), ephemeral_key_public_bytes,
      ephemeral_key_public_bytes + sizeof(ephemeral_key_public_bytes));
  out_response->insert(out_response->end(), my_ciphertext.begin(),
                       my_ciphertext.end());

  std::array<uint8_t, 32> read_key, write_key;
  std::tie(read_key, write_key) = noise.traffic_keys();
  return std::make_unique<Crypter>(read_key, write_key);
}

}  // namespace cablev2
}  // namespace device
