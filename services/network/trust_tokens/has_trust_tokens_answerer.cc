// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/has_trust_tokens_answerer.h"

#include "base/memory/ptr_util.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/trust_tokens/pending_trust_token_store.h"
#include "url/url_constants.h"

namespace network {

std::unique_ptr<HasTrustTokensAnswerer> HasTrustTokensAnswerer::Create(
    const url::Origin& top_frame_origin,
    PendingTrustTokenStore* pending_trust_token_store) {
  DCHECK(pending_trust_token_store);

  if (!IsOriginPotentiallyTrustworthy(top_frame_origin))
    return nullptr;

  if (top_frame_origin.scheme() != url::kHttpsScheme &&
      top_frame_origin.scheme() != url::kHttpScheme) {
    return nullptr;
  }

  return base::WrapUnique(
      new HasTrustTokensAnswerer(top_frame_origin, pending_trust_token_store));
}

HasTrustTokensAnswerer::~HasTrustTokensAnswerer() = default;

void HasTrustTokensAnswerer::HasTrustTokens(const url::Origin& issuer,
                                            HasTrustTokensCallback callback) {
  if (!IsOriginPotentiallyTrustworthy(issuer)) {
    std::move(callback).Run(mojom::HasTrustTokensResult::New(
        mojom::TrustTokenOperationStatus::kInvalidArgument,
        /*has_trust_tokens=*/false));
    return;
  }

  if (issuer.scheme() != url::kHttpsScheme &&
      issuer.scheme() != url::kHttpScheme) {
    std::move(callback).Run(mojom::HasTrustTokensResult::New(
        mojom::TrustTokenOperationStatus::kInvalidArgument,
        /*has_trust_tokens=*/false));
    return;
  }

  pending_trust_token_store_->ExecuteOrEnqueue(
      base::BindOnce(&HasTrustTokensAnswerer::AnswerQueryWithStore,
                     weak_factory_.GetWeakPtr(), issuer, std::move(callback)));
}

void HasTrustTokensAnswerer::AnswerQueryWithStore(
    const url::Origin& issuer,
    HasTrustTokensCallback callback,
    TrustTokenStore* trust_token_store) {
  DCHECK(trust_token_store);

  if (!trust_token_store->SetAssociation(issuer, top_frame_origin_)) {
    std::move(callback).Run(mojom::HasTrustTokensResult::New(
        mojom::TrustTokenOperationStatus::kResourceExhausted,
        /*has_trust_tokens=*/false));
    return;
  }

  bool has_trust_tokens = trust_token_store->CountTokens(issuer);
  std::move(callback).Run(mojom::HasTrustTokensResult::New(
      mojom::TrustTokenOperationStatus::kOk, has_trust_tokens));
}

HasTrustTokensAnswerer::HasTrustTokensAnswerer(
    const url::Origin& top_frame_origin,
    PendingTrustTokenStore* pending_trust_token_store)
    : top_frame_origin_(top_frame_origin),
      pending_trust_token_store_(pending_trust_token_store) {
  DCHECK(pending_trust_token_store);
}

}  // namespace network
