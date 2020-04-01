// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_KEY_COMMITMENTS_H_
#define SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_KEY_COMMITMENTS_H_

#include <map>
#include <memory>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/trust_tokens/suitable_trust_token_origin.h"
#include "services/network/trust_tokens/trust_token_key_commitment_getter.h"

namespace network {

// Class TrustTokenKeyCommitments is a singleton owned by NetworkService; it
// stores all known information about issuers' Trust Tokens key state. This
// state is provided through offline updates via |Set|.
class TrustTokenKeyCommitments : public TrustTokenKeyCommitmentGetter {
 public:
  TrustTokenKeyCommitments();
  ~TrustTokenKeyCommitments() override;

  TrustTokenKeyCommitments(const TrustTokenKeyCommitments&) = delete;
  TrustTokenKeyCommitments& operator=(const TrustTokenKeyCommitments&) = delete;

  // Overwrites the current issuers-to-commitments map with the values in |map|,
  // ignoring those issuer origins which are not suitable Trust Tokens origins
  // (in the sense of SuitableTrustTokenOrigin).
  void Set(
      base::flat_map<url::Origin, mojom::TrustTokenKeyCommitmentResultPtr> map);

  // TrustTokenKeyCommitmentGetter implementation:
  //
  // If |origin| is a suitable Trust Tokens origin (in the sense of
  // SuitableTrustTokenOrigin), searches for a key commitment result
  // corresponding to |origin|. Returns nullptr if |origin| is not suitable, or
  // if no commitment result is found.
  void Get(const url::Origin& origin,
           base::OnceCallback<void(mojom::TrustTokenKeyCommitmentResultPtr)>
               done) const override;

 private:
  base::flat_map<SuitableTrustTokenOrigin,
                 mojom::TrustTokenKeyCommitmentResultPtr>
      map_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_KEY_COMMITMENTS_H_
