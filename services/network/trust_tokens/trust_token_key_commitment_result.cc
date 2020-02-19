// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_key_commitment_result.h"

#include <tuple>

namespace network {

TrustTokenKeyCommitmentResult::TrustTokenKeyCommitmentResult() = default;
TrustTokenKeyCommitmentResult::~TrustTokenKeyCommitmentResult() = default;

TrustTokenKeyCommitmentResult::TrustTokenKeyCommitmentResult(
    const TrustTokenKeyCommitmentResult&) = default;
TrustTokenKeyCommitmentResult::TrustTokenKeyCommitmentResult(
    TrustTokenKeyCommitmentResult&&) = default;
TrustTokenKeyCommitmentResult& TrustTokenKeyCommitmentResult::operator=(
    const TrustTokenKeyCommitmentResult&) = default;
TrustTokenKeyCommitmentResult& TrustTokenKeyCommitmentResult::operator=(
    TrustTokenKeyCommitmentResult&&) = default;

bool operator==(const TrustTokenKeyCommitmentResult::Key& lhs,
                const TrustTokenKeyCommitmentResult::Key& rhs) {
  return std::tie(lhs.body, lhs.expiry, lhs.label) ==
         std::tie(rhs.body, rhs.expiry, rhs.label);
}

bool operator==(const TrustTokenKeyCommitmentResult& lhs,
                const TrustTokenKeyCommitmentResult& rhs) {
  return std::tie(lhs.batch_size, lhs.keys,
                  lhs.signed_redemption_record_verification_key) ==
         std::tie(rhs.batch_size, rhs.keys,
                  rhs.signed_redemption_record_verification_key);
}

}  // namespace network
