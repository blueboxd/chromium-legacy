// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CLIENT_DATA_H_
#define DEVICE_FIDO_CLIENT_DATA_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/strings/string_piece.h"
#include "components/cbor/values.h"
#include "url/origin.h"

namespace device {

// Builds the CollectedClientData[1] dictionary with the given values,
// serializes it to JSON, and returns the resulting string. For legacy U2F
// requests coming from the CryptoToken U2F extension, modifies the object key
// 'type' as required[2].
// [1] https://w3c.github.io/webauthn/#dictdef-collectedclientdata
// [2]
// https://fidoalliance.org/specs/fido-u2f-v1.2-ps-20170411/fido-u2f-raw-message-formats-v1.2-ps-20170411.html#client-data
COMPONENT_EXPORT(DEVICE_FIDO)
std::string SerializeCollectedClientDataToJson(
    const std::string& type,
    const std::string& origin,
    base::span<const uint8_t> challenge,
    bool is_cross_origin,
    bool use_legacy_u2f_type_key = false);

// AndroidClientDataExtensionInput contains data for an extension sent to the
// Clank caBLEv2 authenticator that are required due to the Android FIDO API not
// supporting the CTAP2 clientDataHash input parameter.
struct COMPONENT_EXPORT(DEVICE_FIDO) AndroidClientDataExtensionInput {
  static base::Optional<AndroidClientDataExtensionInput> Parse(
      const cbor::Value& value);

  AndroidClientDataExtensionInput();
  AndroidClientDataExtensionInput(std::string type,
                                  url::Origin origin,
                                  std::vector<uint8_t> challenge);
  AndroidClientDataExtensionInput(const AndroidClientDataExtensionInput&);
  AndroidClientDataExtensionInput(AndroidClientDataExtensionInput&&);

  AndroidClientDataExtensionInput& operator=(
      const AndroidClientDataExtensionInput&);
  AndroidClientDataExtensionInput& operator=(AndroidClientDataExtensionInput&&);

  ~AndroidClientDataExtensionInput();

  std::string type;
  url::Origin origin;
  std::vector<uint8_t> challenge;
};

cbor::Value AsCBOR(const AndroidClientDataExtensionInput& ext);

bool IsValidAndroidClientDataJSON(
    const device::AndroidClientDataExtensionInput& extension_input,
    base::StringPiece android_client_data_json);

}  // namespace device

#endif  // DEVICE_FIDO_CLIENT_DATA_H_
