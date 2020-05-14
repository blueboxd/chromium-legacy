// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/rsa_public_key.h"

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
std::unique_ptr<PublicKey> RSAPublicKey::ExtractFromCOSEKey(
    int32_t algorithm,
    base::span<const uint8_t> cbor_bytes,
    const cbor::Value::MapValue& map) {
  // See https://tools.ietf.org/html/rfc8230#section-4
  cbor::Value::MapValue::const_iterator it =
      map.find(cbor::Value(static_cast<int64_t>(CoseKeyKey::kKty)));
  if (it == map.end() || !it->second.is_integer() ||
      it->second.GetInteger() != static_cast<int64_t>(CoseKeyTypes::kRSA)) {
    return nullptr;
  }

  cbor::Value::MapValue::const_iterator it_n =
      map.find(cbor::Value(static_cast<int64_t>(CoseKeyKey::kRSAModulus)));
  cbor::Value::MapValue::const_iterator it_e = map.find(
      cbor::Value(static_cast<int64_t>(CoseKeyKey::kRSAPublicExponent)));

  if (it_n == map.end() || !it_n->second.is_bytestring() || it_e == map.end() ||
      !it_e->second.is_bytestring()) {
    return nullptr;
  }

  const std::vector<uint8_t>& n(it_n->second.GetBytestring());
  const std::vector<uint8_t>& e(it_e->second.GetBytestring());

  bssl::UniquePtr<BIGNUM> n_bn(BN_new());
  bssl::UniquePtr<BIGNUM> e_bn(BN_new());
  if (!BN_bin2bn(n.data(), n.size(), n_bn.get()) ||
      !BN_bin2bn(e.data(), e.size(), e_bn.get())) {
    return nullptr;
  }

  bssl::UniquePtr<RSA> rsa(RSA_new());
  if (!RSA_set0_key(rsa.get(), n_bn.release(), e_bn.release(), /*d=*/nullptr)) {
    return nullptr;
  }

  bssl::UniquePtr<EVP_PKEY> pkey(EVP_PKEY_new());
  CHECK(EVP_PKEY_assign_RSA(pkey.get(), rsa.release()));

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
