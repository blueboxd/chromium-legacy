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
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/cable/noise.h"
#include "device/fido/fido_constants.h"
#include "third_party/boringssl/src/include/openssl/base.h"

namespace device {
namespace cablev2 {

// NonceAndEID contains both the random nonce chosen for an advert, as well as
// the EID that was generated from it.
typedef std::pair<std::array<uint8_t, device::kCableNonceSize>,
                  std::array<uint8_t, device::kCableEphemeralIdSize>>
    NonceAndEID;

// kP256PointSize is the number of bytes in an X9.62 encoding of a P-256 point.
constexpr size_t kP256PointSize = 65;

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
  bool Decrypt(FidoBleDeviceCommand command,
               base::span<const uint8_t> ciphertext,
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
// response message from the other party.
class COMPONENT_EXPORT(DEVICE_FIDO) HandshakeInitiator {
 public:
  HandshakeInitiator(
      // psk_gen_key is either derived from QR-code secrets or comes from
      // pairing data.
      base::span<const uint8_t, 32> psk_gen_key,
      // nonce is randomly generated per advertisement and ensures that BLE
      // adverts are non-deterministic.
      base::span<const uint8_t, 8> nonce,
      // eid is the EID that was advertised for this handshake. This is checked
      // as part of the handshake.
      base::span<const uint8_t, kCableEphemeralIdSize> eid,
      // peer_identity, if given, specifies that this is a paired handshake
      // and then contains an X9.62, P-256 public key for the peer. Otherwise
      // this is a QR-code handshake.
      base::Optional<base::span<const uint8_t, kP256PointSize>> peer_identity,
      // local_identity must be provided if |peer_identity| is not. It contains
      // the seed for deriving the local identity key.
      base::Optional<base::span<const uint8_t, kCableIdentityKeySeedSize>>
          local_identity);

  ~HandshakeInitiator();

  // BuildInitialMessage returns the handshake message to send to the peer to
  // start a handshake.
  std::vector<uint8_t> BuildInitialMessage();

  // ProcessResponse processes the handshake response from the peer. If
  // successful it returns a |Crypter| for protecting future messages on the
  // connection. If the peer choose to send long-term pairing data, that is also
  // returned.
  base::Optional<std::pair<std::unique_ptr<Crypter>,
                           base::Optional<std::unique_ptr<CableDiscoveryData>>>>
  ProcessResponse(base::span<const uint8_t> response);

 private:
  Noise noise_;
  std::array<uint8_t, 16> eid_;
  std::array<uint8_t, 32> psk_;

  base::Optional<std::array<uint8_t, kP256PointSize>> peer_identity_;
  base::Optional<std::array<uint8_t, kCableIdentityKeySeedSize>> local_seed_;
  bssl::UniquePtr<EC_KEY> ephemeral_key_;
};

// RespondToHandshake responds to a caBLE v2 handshake started by a peer.
COMPONENT_EXPORT(DEVICE_FIDO)
base::Optional<std::unique_ptr<Crypter>> RespondToHandshake(
    // See |HandshakeInitiator| comments about these first three arguments.
    base::span<const uint8_t, 32> psk_gen_key,
    const NonceAndEID& nonce_and_eid,
    // identity, if not nullptr, specifies that this is a paired handshake and
    // contains the long-term identity key for this authenticator.
    const EC_KEY* identity,
    // peer_identity, which must be non-nullptr iff |identity| is nullptr,
    // contains the peer's public key as derived from the QR-code data.
    const EC_POINT* peer_identity,
    // pairing_data, if not nullptr, contains long-term pairing data that will
    // be shared with the peer. This is mutually exclusive with |identity|.
    const CableDiscoveryData* pairing_data,
    // in contains the initial handshake message from the peer.
    base::span<const uint8_t> in,
    // out_response is set to the response handshake message, if successful.
    std::vector<uint8_t>* out_response);

}  // namespace cablev2
}  // namespace device

#endif  // DEVICE_FIDO_CABLE_V2_HANDSHAKE_H_
