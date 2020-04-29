// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/test/signed_request_verification_util.h"

#include "base/containers/span.h"
#include "base/strings/string_piece.h"
#include "net/http/structured_headers.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"

namespace network {
namespace test {

// From the design doc:
//
// The SRR is a two-item Structured Headers Draft 15 dictionary with “byte
// sequence”-typed fields body and signature:
// - body is the serialization of the below CBOR-encoded structure (the “SRR
// body”)
// - signature is the Ed25519 signature, over the SRR body, by the issuer’s
// SRR signing key corresponding to the verification key in the issuer’s key
// commitment registry.
SrrVerificationStatus VerifyTrustTokenSignedRedemptionRecord(
    base::StringPiece record,
    base::StringPiece verification_key) {
  base::Optional<net::structured_headers::Dictionary> maybe_dictionary =
      net::structured_headers::ParseDictionary(record);
  if (!maybe_dictionary)
    return SrrVerificationStatus::kParseError;

  if (maybe_dictionary->size() != 2u)
    return SrrVerificationStatus::kParseError;

  if (!maybe_dictionary->contains("body") ||
      !maybe_dictionary->contains("signature")) {
    return SrrVerificationStatus::kParseError;
  }

  net::structured_headers::Item& body_item =
      maybe_dictionary->at("body").member.front().item;
  if (!body_item.is_byte_sequence())
    return SrrVerificationStatus::kParseError;
  std::string body =
      body_item.GetString();  // GetString gets a byte sequence, too.

  net::structured_headers::Item& signature_item =
      maybe_dictionary->at("signature").member.front().item;
  if (!signature_item.is_byte_sequence())
    return SrrVerificationStatus::kParseError;
  std::string signature =
      signature_item.GetString();  // GetString gets a byte sequence, too.

  if (verification_key.size() != ED25519_PUBLIC_KEY_LEN)
    return SrrVerificationStatus::kSignatureVerificationError;

  if (signature.size() != ED25519_SIGNATURE_LEN)
    return SrrVerificationStatus::kSignatureVerificationError;

  if (!ED25519_verify(
          base::as_bytes(base::make_span(body)).data(), body.size(),
          base::as_bytes(base::make_span<ED25519_SIGNATURE_LEN>(signature))
              .data(),
          base::as_bytes(
              base::make_span<ED25519_PUBLIC_KEY_LEN>(verification_key))
              .data())) {
    return SrrVerificationStatus::kSignatureVerificationError;
  }

  return SrrVerificationStatus::kSuccess;
}

}  // namespace test
}  // namespace network
