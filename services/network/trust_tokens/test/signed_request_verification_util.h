// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_TEST_SIGNED_REQUEST_VERIFICATION_UTIL_H_
#define SERVICES_NETWORK_TRUST_TOKENS_TEST_SIGNED_REQUEST_VERIFICATION_UTIL_H_

#include "base/strings/string_piece_forward.h"

namespace network {
namespace test {

// Parses the given Trust Tokens signed redemption record
// https://docs.google.com/document/d/1TNnya6B8pyomDK2F1R9CL3dY10OAmqWlnCxsWyOBDVQ/edit#bookmark=id.omg78vbnmjid,
// extracts the signature and body, and uses the given verification key to
// verify the signature.
enum class SrrVerificationStatus {
  kParseError,
  kSignatureVerificationError,
  kSuccess
};
SrrVerificationStatus VerifyTrustTokenSignedRedemptionRecord(
    base::StringPiece record,
    base::StringPiece verification_key);

}  // namespace test
}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_TEST_SIGNED_REQUEST_VERIFICATION_UTIL_H_
