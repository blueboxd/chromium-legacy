// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/test/signed_request_verification_util.h"

#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/strings/string_split.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "net/http/http_request_headers.h"
#include "net/http/structured_headers.h"
#include "services/network/trust_tokens/ed25519_trust_token_request_signer.h"
#include "services/network/trust_tokens/signed_redemption_record_serialization.h"
#include "services/network/trust_tokens/trust_token_http_headers.h"
#include "services/network/trust_tokens/trust_token_parameterization.h"
#include "services/network/trust_tokens/trust_token_request_canonicalizer.h"
#include "services/network/trust_tokens/trust_token_request_signing_helper.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"

namespace network {
namespace test {
namespace {

base::Optional<base::flat_map<std::string, net::structured_headers::Item>>
DeserializeSecSignatureHeader(base::StringPiece header) {
  base::Optional<net::structured_headers::Dictionary> maybe_dictionary =
      net::structured_headers::ParseDictionary(header);
  if (!maybe_dictionary)
    return base::nullopt;

  base::flat_map<std::string, net::structured_headers::Item> ret;
  for (const auto& kv : *maybe_dictionary) {
    ret[kv.first] = kv.second.member.front().item;
  }

  return ret;
}

}  // namespace

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
    base::StringPiece verification_key,
    std::string* srr_body_out) {
  std::string body, signature;
  if (!ParseTrustTokenSignedRedemptionRecord(record, &body, &signature))
    return SrrVerificationStatus::kParseError;

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

  if (srr_body_out)
    srr_body_out->swap(body);
  return SrrVerificationStatus::kSuccess;
}

bool ReconstructSigningDataAndVerifySignature(
    const GURL& destination,
    const net::HttpRequestHeaders& headers,
    base::OnceCallback<bool(base::span<const uint8_t> data,
                            base::span<const uint8_t> signature,
                            base::span<const uint8_t> verification_key)>
        verifier,
    std::string* error_out,
    std::string* verification_key_out) {
  // Make it possible to set the error without needing to check for
  // |error_out|'s presence.
  std::string dummy_error;
  if (!error_out)
    error_out = &dummy_error;

  std::string signature_header;
  if (!headers.GetHeader(kTrustTokensRequestHeaderSecSignature,
                         &signature_header)) {
    *error_out = "Missing Sec-Signature header";
    return false;
  }

  base::Optional<base::flat_map<std::string, net::structured_headers::Item>>
      map = DeserializeSecSignatureHeader(signature_header);
  if (!map) {
    *error_out = "Malformed Sec-Signature header";
    return false;
  }

  auto it = map->find("sig");
  if (it == map->end()) {
    *error_out = "Missing 'sig' element in the Sec-Signature header";
    return false;
  }
  if (!it->second.is_byte_sequence()) {
    *error_out = "'sig' element in Sec-Signature header is type-unsafe";
    return false;
  }
  // GetString is also the method one uses to get a byte sequence.
  base::StringPiece signature = it->second.GetString();

  it = map->find("public-key");
  if (it == map->end()) {
    *error_out = "Missing 'public-key' element in the Sec-Signature header";
    return false;
  }
  if (!it->second.is_byte_sequence()) {
    *error_out = "'public-key' element in Sec-Signature header is type-unsafe";
    return false;
  }
  base::StringPiece public_key = it->second.GetString();

  it = map->find("sign-request-data");
  if (it == map->end()) {
    *error_out =
        "Missing 'sign-request-data' element in the Sec-Signature header";
    return false;
  }
  if (!it->second.is_token()) {
    *error_out =
        "'sign-request-data' element in Sec-Signature header is type-unsafe";
    return false;
  }
  // GetString is also the method one uses to get a token.
  base::StringPiece sign_request_data = it->second.GetString();

  if (sign_request_data != "headers-only" && sign_request_data != "include") {
    *error_out =
        "'sign-request-data' element in Sec-Signature header had a bad value";
    return false;
  }

  base::Optional<std::vector<uint8_t>> written_reconstructed_cbor =
      TrustTokenRequestCanonicalizer().Canonicalize(
          destination, headers, public_key,
          sign_request_data == "include"
              ? mojom::TrustTokenSignRequestData::kInclude
              : mojom::TrustTokenSignRequestData::kHeadersOnly);
  if (!written_reconstructed_cbor) {
    *error_out = "Error reconstructing canonical request data";
    return false;
  }

  std::vector<uint8_t> reconstructed_signing_data(
      std::begin(
          TrustTokenRequestSigningHelper::kRequestSigningDomainSeparator),
      std::end(TrustTokenRequestSigningHelper::kRequestSigningDomainSeparator));
  reconstructed_signing_data.insert(reconstructed_signing_data.end(),
                                    written_reconstructed_cbor->begin(),
                                    written_reconstructed_cbor->end());

  if (!verifier) {
    verifier =
        base::BindOnce(&Ed25519TrustTokenRequestSigner::Verify,
                       std::make_unique<Ed25519TrustTokenRequestSigner>());
  }

  if (!std::move(verifier).Run(base::make_span(reconstructed_signing_data),
                               base::as_bytes(base::make_span(signature)),
                               base::as_bytes(base::make_span(public_key)))) {
    *error_out = "Error verifying signature";
    return false;
  }

  if (verification_key_out)
    *verification_key_out = std::string(public_key);
  return true;
}

bool ConfirmSrrBodyIntegrity(base::StringPiece srr_body,
                             std::string* error_out) {
  std::string dummy_error;
  std::string& error = error_out ? *error_out : dummy_error;

  base::Optional<cbor::Value> maybe_map =
      cbor::Reader::Read(base::as_bytes(base::make_span(srr_body)));

  if (!maybe_map) {
    error = "SRR body wasn't valid CBOR";
    return false;
  }

  if (!maybe_map->is_map()) {
    error = "SRR body wasn't a CBOR map";
    return false;
  }

  const cbor::Value::MapValue& map = maybe_map->GetMap();

  if (map.size() != 3) {
    error = "SRR body is a map of unexpected size";
    return false;
  }

  // check_field is a convenience function automating some of the work of
  // verifying that the CBOR map has the desired structure. It takes a (possibly
  // two-level compound) field name and a type-checker cbor::Value member
  // function pointer (e.g. &cbor::Value::is_string) and verifies that the field
  // exists and satisfies the given type predicate.
  auto check_field = [&](base::StringPiece key, auto type_checker) -> bool {
    const cbor::Value::MapValue* submap = &map;
    if (base::Contains(key, ".")) {
      auto keys = base::SplitStringPiece(key, ".", base::KEEP_WHITESPACE,
                                         base::SPLIT_WANT_ALL);
      cbor::Value submap_key(keys[0], cbor::Value::Type::STRING);
      if (!map.contains(submap_key) || !map.at(submap_key).is_map()) {
        return false;
      }

      submap = &map.at(submap_key).GetMap();
      key = keys[1];
    }

    cbor::Value cbor_key(key, cbor::Value::Type::STRING);
    return submap->contains(cbor_key) && (submap->at(cbor_key).*type_checker)();
  };

  for (const auto& tup : {
           std::make_tuple("client-data", &cbor::Value::is_map),
           std::make_tuple("client-data.key-hash", &cbor::Value::is_bytestring),
           std::make_tuple("client-data.redemption-timestamp",
                           &cbor::Value::is_unsigned),
           std::make_tuple("client-data.redeeming-origin",
                           &cbor::Value::is_string),
           std::make_tuple("metadata", &cbor::Value::is_map),
           std::make_tuple("metadata.public", &cbor::Value::is_unsigned),
           std::make_tuple("metadata.private", &cbor::Value::is_unsigned),
           std::make_tuple("expiry-timestamp", &cbor::Value::is_unsigned),
       }) {
    if (!check_field(std::get<0>(tup), std::get<1>(tup))) {
      error = "Missing or type-unsafe " + std::string(std::get<0>(tup));
      return false;
    }
  }

  return true;
}

}  // namespace test
}  // namespace network
