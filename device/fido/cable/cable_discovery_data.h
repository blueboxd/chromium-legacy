// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CABLE_CABLE_DISCOVERY_DATA_H_
#define DEVICE_FIDO_CABLE_CABLE_DISCOVERY_DATA_H_

#include <stdint.h>
#include <array>

#include "base/component_export.h"
#include "base/containers/span.h"

namespace device {

constexpr size_t kCableEphemeralIdSize = 16;
constexpr size_t kCableSessionPreKeySize = 32;
constexpr size_t kCableQRSecretSize = 16;

using CableEidArray = std::array<uint8_t, kCableEphemeralIdSize>;
using CableSessionPreKeyArray = std::array<uint8_t, kCableSessionPreKeySize>;
// QRGeneratorKey is a random, AES-256 key that is used by
// |CableDiscoveryData::DeriveQRKeyMaterial| to encrypt a coarse timestamp and
// generate QR secrets, EIDs, etc.
using QRGeneratorKey = std::array<uint8_t, 32>;

// Encapsulates information required to discover Cable device per single
// credential. When multiple credentials are enrolled to a single account
// (i.e. more than one phone has been enrolled to an user account as a
// security key), then FidoCableDiscovery must advertise for all of the client
// EID received from the relying party.
// TODO(hongjunchoi): Add discovery data required for MakeCredential request.
// See: https://crbug.com/837088
struct COMPONENT_EXPORT(DEVICE_FIDO) CableDiscoveryData {
  enum class Version {
    INVALID,
    V1,
    V2,
  };

  CableDiscoveryData(Version version,
                     const CableEidArray& client_eid,
                     const CableEidArray& authenticator_eid,
                     const CableSessionPreKeyArray& session_pre_key);
  CableDiscoveryData();
  CableDiscoveryData(const CableDiscoveryData& data);
  ~CableDiscoveryData();

  CableDiscoveryData& operator=(const CableDiscoveryData& other);
  bool operator==(const CableDiscoveryData& other) const;

  // NewQRKey returns a random key for QR generation.
  static QRGeneratorKey NewQRKey();

  // CurrentTimeTick returns the current time as used by QR generation. The size
  // of these ticks is a purely local matter for Chromium.
  static int64_t CurrentTimeTick();

  // DeriveQRKeyMaterial writes |kCableQRSecretSize| bytes to |out_qr_secret|,
  // |kCableEphemeralIdSize| bytes to |out_authenticator_eid| and
  // |kCableSessionPreKeySize| bytes of |out_session_key|, based on the given
  // generator key and current time.
  static void DeriveQRKeyMaterial(
      base::span<uint8_t, kCableQRSecretSize> out_qr_secret,
      base::span<uint8_t, kCableEphemeralIdSize> out_authenticator_eid,
      base::span<uint8_t, kCableSessionPreKeySize> out_session_key,
      base::span<const uint8_t, 32> qr_generator_key,
      const int64_t tick);

  // version indicates whether v1 or v2 data is contained in this object.
  // |INVALID| is not a valid version but is set as the default to catch any
  // cases where the version hasn't been set explicitly.
  Version version = Version::INVALID;

  struct V1Data {
    CableEidArray client_eid;
    CableEidArray authenticator_eid;
    CableSessionPreKeyArray session_pre_key;
  };
  base::Optional<V1Data> v1;
};

}  // namespace device

#endif  // DEVICE_FIDO_CABLE_CABLE_DISCOVERY_DATA_H_
