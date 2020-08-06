// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CABLE_V2_HANDSHAKE_H_
#define DEVICE_FIDO_CABLE_V2_HANDSHAKE_H_

#include <stdint.h>

#include <array>
#include <memory>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/optional.h"
#include "components/cbor/values.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/cable/noise.h"
#include "device/fido/fido_constants.h"
#include "third_party/boringssl/src/include/openssl/base.h"

namespace device {
namespace cablev2 {

// EncodePaddedCBORMap encodes the given map and pads it to 256 bytes in such a
// way that |DecodePaddedCBORMap| can decode it. The padding is done on the
// assumption that the returned bytes will be encrypted and the encoded size of
// the map should be hidden. The function can fail if the CBOR encoding fails
// or, somehow, the size overflows.
COMPONENT_EXPORT(DEVICE_FIDO)
base::Optional<std::vector<uint8_t>> EncodePaddedCBORMap(
    cbor::Value::MapValue map);

// DecodePaddedCBORMap unpads and decodes a CBOR map as produced by
// |EncodePaddedCBORMap|.
COMPONENT_EXPORT(DEVICE_FIDO)
base::Optional<cbor::Value> DecodePaddedCBORMap(
    base::span<const uint8_t> input);

// NonceAndEID contains both the random nonce chosen for an advert, as well as
// the EID that was generated from it.
constexpr size_t kNonceSize = 10;
typedef std::pair<std::array<uint8_t, kNonceSize>,
                  std::array<uint8_t, device::kCableEphemeralIdSize>>
    NonceAndEID;

// Crypter handles the post-handshake encryption of CTAP2 messages.
class COMPONENT_EXPORT(DEVICE_FIDO) Crypter {
 public:
  Crypter(base::span<const uint8_t, 32> read_key,
          base::span<const uint8_t, 32> write_key);
  ~Crypter();

  // Encrypt encrypts |message_to_encrypt| and overrides it with the
  // ciphertext. It returns true on success and false on error.
  bool Encrypt(std::vector<uint8_t>* message_to_encrypt);

  // Decrypt decrypts |ciphertext|, which was received as the payload of a
  // message with the given command, and writes the plaintext to
  // |out_plaintext|. It returns true on success and false on error.
  //
  // (In practice, command must always be |kMsg|. But passing it here makes it
  // less likely that other code will forget to check that.)
  bool Decrypt(base::span<const uint8_t> ciphertext,
               std::vector<uint8_t>* out_plaintext);

  // IsCounterpartyOfForTesting returns true if |other| is the mirror-image of
  // this object. (I.e. read/write keys are equal but swapped.)
  bool IsCounterpartyOfForTesting(const Crypter& other) const;

 private:
  const std::array<uint8_t, 32> read_key_, write_key_;
  uint32_t read_sequence_num_ = 0;
  uint32_t write_sequence_num_ = 0;
};

// HandshakeInitiator starts a caBLE v2 handshake and processes the single
// response message from the other party. The handshake is always initiated from
// the phone.
class COMPONENT_EXPORT(DEVICE_FIDO) HandshakeInitiator {
 public:
  HandshakeInitiator(
      // psk_gen_key is either derived from QR-code secrets or comes from
      // pairing data.
      base::span<const uint8_t, 32> psk_gen_key,
      // nonce is randomly generated per advertisement and ensures that BLE
      // adverts are non-deterministic.
      base::span<const uint8_t, kNonceSize> nonce,
      // peer_identity, if not nullopt, specifies that this is a QR handshake
      // and then contains a P-256 public key for the peer. Otherwise this is a
      // paired handshake.
      base::Optional<base::span<const uint8_t, kP256X962Length>> peer_identity,
      // local_identity must be provided iff |peer_identity| is not. It contains
      // the local identity key.
      bssl::UniquePtr<EC_KEY> local_identity);

  ~HandshakeInitiator();

  // BuildInitialMessage returns the handshake message to send to the peer to
  // start a handshake.
  std::vector<uint8_t> BuildInitialMessage(
      // eid is the EID that was advertised for this handshake. This is checked
      // as part of the handshake.
      base::span<const uint8_t, kCableEphemeralIdSize> eid,
      // getinfo contains the CBOR-serialised getInfo response for this
      // authenticator. This is assumed not to contain highly-sensitive
      // information and is included to avoid an extra round-trip. (It is
      // encrypted but an attacker who could eavesdrop on the tunnel connection
      // and observe the QR code could obtain it.)
      base::span<const uint8_t> get_info_bytes);

  // ProcessResponse processes the handshake response from the peer. If
  // successful it returns a |Crypter| for protecting future messages on the
  // connection.
  base::Optional<std::unique_ptr<Crypter>> ProcessResponse(
      base::span<const uint8_t> response);

 private:
  Noise noise_;
  std::array<uint8_t, 32> psk_;

  base::Optional<std::array<uint8_t, kP256X962Length>> peer_identity_;
  bssl::UniquePtr<EC_KEY> local_identity_;
  bssl::UniquePtr<EC_KEY> ephemeral_key_;
};

// RespondToHandshake responds to a caBLE v2 handshake started by a peer. It
// returns a Crypter for encrypting and decrypting future messages, as well as
// the getInfo response from the phone.
COMPONENT_EXPORT(DEVICE_FIDO)
base::Optional<std::pair<std::unique_ptr<Crypter>, std::vector<uint8_t>>>
RespondToHandshake(
    // For the first two arguments see |HandshakeInitiator| comments about
    // |psk_gen_key| and |nonce|, and the |BuildInitialMessage| comment about
    // |eid|.
    base::span<const uint8_t, 32> psk_gen_key,
    const NonceAndEID& nonce_and_eid,
    // identity_seed, if not nullopt, specifies that this is a QR handshake and
    // contains the seed for QR key for this client.
    base::Optional<base::span<const uint8_t, kCableIdentityKeySeedSize>>
        identity_seed,
    // peer_identity, which must be non-nullopt iff |identity| is nullopt,
    // contains the peer's public key as taken from the pairing data.
    base::Optional<base::span<const uint8_t, kP256X962Length>> peer_identity,
    // in contains the initial handshake message from the peer.
    base::span<const uint8_t> in,
    // out_response is set to the response handshake message, if successful.
    std::vector<uint8_t>* out_response);

}  // namespace cablev2
}  // namespace device

#endif  // DEVICE_FIDO_CABLE_V2_HANDSHAKE_H_
