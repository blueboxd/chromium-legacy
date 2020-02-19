// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_KEY_COMMITMENT_RESULT_H_
#define SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_KEY_COMMITMENT_RESULT_H_

#include <string>
#include <vector>

#include "base/optional.h"
#include "base/time/time.h"

namespace network {

// Struct TrustTokenKeyCommitmentResult represents a Trust Token issuer's
// current key commitments and associated information provided through the key
// commitment mechanism.
struct TrustTokenKeyCommitmentResult final {
  TrustTokenKeyCommitmentResult();
  ~TrustTokenKeyCommitmentResult();

  TrustTokenKeyCommitmentResult(const TrustTokenKeyCommitmentResult&);
  TrustTokenKeyCommitmentResult(TrustTokenKeyCommitmentResult&&);

  TrustTokenKeyCommitmentResult& operator=(
      const TrustTokenKeyCommitmentResult&);
  TrustTokenKeyCommitmentResult& operator=(TrustTokenKeyCommitmentResult&&);

  struct Key {
    std::string body;
    base::Time expiry;
    uint32_t label;
  };
  // |keys| is the collection of the issuer's current key commitments.
  std::vector<Key> keys;

  // |batch_size| is the issuer's optional number of tokens it wishes the client
  // to request per Trust Tokens issuance operation.
  base::Optional<int> batch_size;

  // |signed_redemption_record_verification_key| is an Ed25519 public key that
  // can be used to verify Signed Redemption Record (SRR) signatures
  // subsequently provided by the issuer.
  std::string signed_redemption_record_verification_key;
};

// For testing.
bool operator==(const TrustTokenKeyCommitmentResult::Key& lhs,
                const TrustTokenKeyCommitmentResult::Key& rhs);
bool operator==(const TrustTokenKeyCommitmentResult& lhs,
                const TrustTokenKeyCommitmentResult& rhs);

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_KEY_COMMITMENT_RESULT_H_
