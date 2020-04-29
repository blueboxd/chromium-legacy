// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_TEST_SIGNED_REQUEST_VERIFICATION_UTIL_H_
#define SERVICES_NETWORK_TRUST_TOKENS_TEST_SIGNED_REQUEST_VERIFICATION_UTIL_H_

#include <string>

#include "base/callback.h"
#include "base/containers/span.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "net/http/http_request_headers.h"
#include "url/gurl.h"

namespace network {
namespace test {

// Parses the given Trust Tokens signed redemption record
// https://docs.google.com/document/d/1TNnya6B8pyomDK2F1R9CL3dY10OAmqWlnCxsWyOBDVQ/edit#bookmark=id.omg78vbnmjid,
// extracts the signature and body, and uses the given verification key to
// verify the signature.
//
// On success, if |srr_body_out| is non-null, sets |srr_body_out| to the
// obtained SRR body.
enum class SrrVerificationStatus {
  kParseError,
  kSignatureVerificationError,
  kSuccess
};
SrrVerificationStatus VerifyTrustTokenSignedRedemptionRecord(
    base::StringPiece record,
    base::StringPiece verification_key,
    std::string* srr_body_out = nullptr);

// Reconstructs a request's canonical request data, extracts the signature from
// its Sec-Signature header, checks that the Sec-Signature header's contained
// signature verifies.
//
// Optionally:
// - if |verification_key_out| is non-null, on success, returns the verification
// key so that the caller can verify further state concerning the key (like
// confirming that the key was bound to a previous redemption).
// - if |error_out| is non-null, on failure, sets it to a human-readable
// description of the reason the verification failed.
// - if |verifier| is non-null, uses the given verifier to verify the
// signature instead of Ed25519
bool ReconstructSigningDataAndVerifySignature(
    const GURL& destination,
    const net::HttpRequestHeaders& headers,
    base::OnceCallback<bool(base::span<const uint8_t> data,
                            base::span<const uint8_t> signature,
                            base::span<const uint8_t> verification_key)>
        verifier = {},  // defaults to Ed25519
    std::string* error_out = nullptr,
    std::string* verification_key_out = nullptr);

// Returns true if |srr_body| a valid CBOR encoding of an "SRR body" struct, as
// defined in the design doc. Otherwise, returns false and, if |error_out| is
// non-null, sets |error_out| to a helpful error message.
bool ConfirmSrrBodyIntegrity(base::StringPiece srr_body,
                             std::string* error_out = nullptr);

}  // namespace test
}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_TEST_SIGNED_REQUEST_VERIFICATION_UTIL_H_
