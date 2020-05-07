// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_PUBLIC_KEY_H_
#define DEVICE_FIDO_PUBLIC_KEY_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/macros.h"

namespace device {

// https://www.w3.org/TR/webauthn/#credentialpublickey
class COMPONENT_EXPORT(DEVICE_FIDO) PublicKey {
 public:
  PublicKey(int32_t algorithm, base::span<const uint8_t> cbor_bytes);
  virtual ~PublicKey();

  // algorithm returns the COSE algorithm identifier for this public key.
  int32_t algorithm() const;

  // The credential public key as a COSE_Key map as defined in Section 7
  // of https://tools.ietf.org/html/rfc8152.
  const std::vector<uint8_t>& cose_key_bytes() const;

 private:
  const int32_t algorithm_;
  std::vector<uint8_t> cbor_bytes_;

  DISALLOW_COPY_AND_ASSIGN(PublicKey);
};

}  // namespace device

#endif  // DEVICE_FIDO_PUBLIC_KEY_H_
