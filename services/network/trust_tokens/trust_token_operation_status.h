// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_OPERATION_STATUS_H_
#define SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_OPERATION_STATUS_H_

namespace network {

// TrustTokenOperationStatus enumerates (an incomplete collection of) outcomes
// for the Trust Tokens (http://github.com/WICG/trust-token-api) protocol
// operation: token issuance, token redemption, and request signing.
//
// Each status may be returned in similar cases beyond those listed in its
// comment.
enum class TrustTokenOperationStatus {
  kOk,

  // A client-provided argument was malformed or otherwise invalid.
  kInvalidArgument,

  // A precondition failed (for instance, a rate limit would be exceeded, a key
  // commitment check failed, or executing the operation would cause too many
  // issuers to be associated with the operation's top-level origin).
  kFailedPrecondition,

  // No inputs for the given operation available, or a quota on the operation's
  // output would be exceeded.
  kResourceExhausted,

  // The operation's result already exists (for instance, a cache was hit).
  kAlreadyExists,

  // Internal storage, or some other necessary resource, has not yet
  // initialized or has become unavailable.
  kUnavailable,

  // The server response was malformed or otherwise invalid.
  kBadResponse,

  // A, usually severe, internal error occurred.
  kInternalError,

  // The operation failed for some other reason.
  kUnknownError,

  // Sentinel used for serialization in IPC_ENUM_TRAITS and/or logging; do not
  // use directly.
  kMaxValue = kUnknownError,
};

}  // namespace network
#endif  // SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_OPERATION_STATUS_H_
