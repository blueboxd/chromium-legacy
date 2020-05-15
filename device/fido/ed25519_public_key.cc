// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/ed25519_public_key.h"

#include <utility>

#include "components/cbor/writer.h"
#include "device/fido/fido_constants.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/obj.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"

namespace device {

// static
std::unique_ptr<PublicKey> Ed25519PublicKey::ExtractFromCOSEKey(
    int32_t algorithm,
    base::span<const uint8_t> cbor_bytes,
    const cbor::Value::MapValue& map) {
  // See https://tools.ietf.org/html/rfc8152#section-13.2
  cbor::Value::MapValue::const_iterator it =
      map.find(cbor::Value(static_cast<int64_t>(CoseKeyKey::kKty)));
  if (it == map.end() || !it->second.is_integer() ||
      it->second.GetInteger() != static_cast<int64_t>(CoseKeyTypes::kOKP)) {
    return nullptr;
  }

  it = map.find(cbor::Value(static_cast<int64_t>(CoseKeyKey::kEllipticCurve)));
  if (it == map.end() || !it->second.is_integer() ||
      it->second.GetInteger() != static_cast<int64_t>(CoseCurves::kEd25519)) {
    return nullptr;
  }

  it = map.find(cbor::Value(static_cast<int64_t>(CoseKeyKey::kEllipticX)));
  if (it == map.end() || !it->second.is_bytestring()) {
    return nullptr;
  }

  // The COSE RFC says that "This contains the x-coordinate for the EC point".
  // The RFC authors do not appear to understand what's going on because it
  // actually just contains the Ed25519 public key, which you would expect, and
  // which also encodes the y-coordinate as a sign bit.
  const std::vector<uint8_t>& key = it->second.GetBytestring();
  if (key.size() != 32) {
    return nullptr;
  }

  // We could attempt to check whether |key| contains a quadratic residue, as it
  // should. But that would involve diving into the guts of Ed25519 too much.

  bssl::UniquePtr<EVP_PKEY> pkey(EVP_PKEY_new_raw_public_key(
      EVP_PKEY_ED25519, /*engine=*/nullptr, key.data(), key.size()));
  if (!pkey) {
    return nullptr;
  }

  bssl::ScopedCBB cbb;
  uint8_t* der_bytes = nullptr;
  size_t der_bytes_len = 0;
  CHECK(CBB_init(cbb.get(), /* initial size */ 128) &&
        EVP_marshal_public_key(cbb.get(), pkey.get()) &&
        CBB_finish(cbb.get(), &der_bytes, &der_bytes_len));

  std::vector<uint8_t> der_bytes_vec(der_bytes, der_bytes + der_bytes_len);
  OPENSSL_free(der_bytes);

  return std::make_unique<PublicKey>(algorithm, cbor_bytes,
                                     std::move(der_bytes_vec));
}

}  // namespace device
