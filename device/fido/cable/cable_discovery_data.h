// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CABLE_CABLE_DISCOVERY_DATA_H_
#define DEVICE_FIDO_CABLE_CABLE_DISCOVERY_DATA_H_

#include <stdint.h>
#include <array>

#include "base/component_export.h"

namespace device {

constexpr size_t kEphemeralIdSize = 16;
constexpr size_t kSessionPreKeySize = 32;

using EidArray = std::array<uint8_t, kEphemeralIdSize>;
using SessionPreKeyArray = std::array<uint8_t, kSessionPreKeySize>;

// Encapsulates information required to discover Cable device per single
// credential. When multiple credentials are enrolled to a single account
// (i.e. more than one phone has been enrolled to an user account as a
// security key), then FidoCableDiscovery must advertise for all of the client
// EID received from the relying party.
// TODO(hongjunchoi): Add discovery data required for MakeCredential request.
// See: https://crbug.com/837088
struct COMPONENT_EXPORT(DEVICE_FIDO) CableDiscoveryData {
  CableDiscoveryData(uint8_t version,
                     const EidArray& client_eid,
                     const EidArray& authenticator_eid,
                     const SessionPreKeyArray& session_pre_key);
  CableDiscoveryData();
  CableDiscoveryData(const CableDiscoveryData& data);
  CableDiscoveryData& operator=(const CableDiscoveryData& other);
  bool operator==(const CableDiscoveryData& other) const;
  ~CableDiscoveryData();

  uint8_t version;
  EidArray client_eid;
  EidArray authenticator_eid;
  SessionPreKeyArray session_pre_key;
};

}  // namespace device

#endif  // DEVICE_FIDO_CABLE_CABLE_DISCOVERY_DATA_H_
