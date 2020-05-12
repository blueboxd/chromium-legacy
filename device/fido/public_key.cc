// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/public_key.h"

#include <vector>
#include "base/containers/span.h"
#include "base/optional.h"
#include "device/fido/fido_parsing_utils.h"

namespace device {

PublicKey::~PublicKey() = default;

PublicKey::PublicKey(int32_t algorithm,
                     base::span<const uint8_t> cbor_bytes,
                     base::Optional<std::vector<uint8_t>> der_bytes)
    : algorithm_(algorithm),
      cbor_bytes_(fido_parsing_utils::Materialize(cbor_bytes)),
      der_bytes_(std::move(der_bytes)) {}

int32_t PublicKey::algorithm() const {
  return algorithm_;
}

const std::vector<uint8_t>& PublicKey::cose_key_bytes() const {
  return cbor_bytes_;
}

const base::Optional<std::vector<uint8_t>>& PublicKey::der_bytes() const {
  return der_bytes_;
}

}  // namespace device
