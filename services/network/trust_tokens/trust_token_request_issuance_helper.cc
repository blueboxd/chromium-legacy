// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_request_issuance_helper.h"

#include <utility>

#include "base/callback.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/trust_tokens/proto/public.pb.h"
#include "services/network/trust_tokens/trust_token_http_headers.h"
#include "services/network/trust_tokens/trust_token_parameterization.h"
#include "services/network/trust_tokens/trust_token_store.h"
#include "services/network/trust_tokens/types.h"
#include "url/url_constants.h"

namespace network {

TrustTokenRequestIssuanceHelper::TrustTokenRequestIssuanceHelper(
    const url::Origin& top_level_origin,
    TrustTokenStore* token_store,
    std::unique_ptr<TrustTokenKeyCommitmentGetter> key_commitment_getter,
    std::unique_ptr<Cryptographer> cryptographer)
    : top_level_origin_(top_level_origin),
      token_store_(token_store),
      key_commitment_getter_(std::move(key_commitment_getter)),
      cryptographer_(std::move(cryptographer)) {
  DCHECK(top_level_origin.scheme() == url::kHttpsScheme ||
         (top_level_origin.scheme() == url::kHttpScheme &&
          IsOriginPotentiallyTrustworthy(top_level_origin)))
      << top_level_origin;

  DCHECK(token_store_);
  DCHECK(key_commitment_getter_);
  DCHECK(cryptographer_);
}

TrustTokenRequestIssuanceHelper::~TrustTokenRequestIssuanceHelper() = default;

TrustTokenRequestIssuanceHelper::Cryptographer::UnblindedTokens::
    UnblindedTokens() = default;
TrustTokenRequestIssuanceHelper::Cryptographer::UnblindedTokens::
    ~UnblindedTokens() = default;

void TrustTokenRequestIssuanceHelper::Begin(
    net::URLRequest* request,
    base::OnceCallback<void(mojom::TrustTokenOperationStatus)> done) {
  DCHECK(request->url().SchemeIsHTTPOrHTTPS() &&
         IsUrlPotentiallyTrustworthy(request->url()))
      << request->url();
  DCHECK(request->initiator() &&
             request->initiator()->scheme() == url::kHttpsScheme ||
         (request->initiator()->scheme() == url::kHttpScheme &&
          IsOriginPotentiallyTrustworthy(*request->initiator())))
      << (request->initiator() ? request->initiator()->Serialize()
                               : "(missing)");

  issuer_ = url::Origin::Create(request->url());
  if (!token_store_->SetAssociation(issuer_, top_level_origin_)) {
    std::move(done).Run(mojom::TrustTokenOperationStatus::kResourceExhausted);
    return;
  }

  if (token_store_->CountTokens(issuer_) == kTrustTokenPerIssuerTokenCapacity) {
    std::move(done).Run(mojom::TrustTokenOperationStatus::kResourceExhausted);
    return;
  }

  key_commitment_getter_->Get(
      issuer_,
      base::BindOnce(&TrustTokenRequestIssuanceHelper::OnGotKeyCommitment,
                     weak_ptr_factory_.GetWeakPtr(), request, std::move(done)));
}

void TrustTokenRequestIssuanceHelper::OnGotKeyCommitment(
    net::URLRequest* request,
    base::OnceCallback<void(mojom::TrustTokenOperationStatus)> done,
    mojom::TrustTokenKeyCommitmentResultPtr commitment_result) {
  if (!commitment_result) {
    std::move(done).Run(mojom::TrustTokenOperationStatus::kFailedPrecondition);
    return;
  }

  for (const mojom::TrustTokenVerificationKeyPtr& key :
       commitment_result->keys) {
    if (!cryptographer_->AddKey(key->body)) {
      std::move(done).Run(
          mojom::TrustTokenOperationStatus::kFailedPrecondition);
      return;
    }
  }

  // Evict tokens signed with keys other than those from the issuer's most
  // recent commitments.
  token_store_->PruneStaleIssuerState(issuer_, commitment_result->keys);

  int batch_size = commitment_result->batch_size
                       ? std::min(commitment_result->batch_size->value,
                                  kMaximumTrustTokenIssuanceBatchSize)
                       : kDefaultTrustTokenIssuanceBatchSize;

  base::Optional<std::string> maybe_blinded_tokens =
      cryptographer_->BeginIssuance(batch_size);

  if (!maybe_blinded_tokens) {
    std::move(done).Run(mojom::TrustTokenOperationStatus::kInternalError);
    return;
  }
  request->SetExtraRequestHeaderByName(kTrustTokensSecTrustTokenHeader,
                                       std::move(*maybe_blinded_tokens),
                                       /*overwrite=*/true);

  // We don't want cache reads, because the highest priority is to execute the
  // protocol operation by sending the server the Trust Tokens request header
  // and getting the corresponding response header, but we want cache writes
  // in case subsequent requests are made to the same URL in non-trust-token
  // settings.
  request->SetLoadFlags(request->load_flags() | net::LOAD_BYPASS_CACHE);

  std::move(done).Run(mojom::TrustTokenOperationStatus::kOk);
}

mojom::TrustTokenOperationStatus TrustTokenRequestIssuanceHelper::Finalize(
    mojom::URLResponseHead* response) {
  DCHECK(response);

  // A response headers object should be present on all responses for
  // https-scheme requests (which Trust Tokens requests are).
  DCHECK(response->headers);

  std::string header_value;

  // EnumerateHeader(|iter|=nullptr) asks for the first instance of the header,
  // if any.
  if (!response->headers->EnumerateHeader(
          /*iter=*/nullptr, kTrustTokensSecTrustTokenHeader, &header_value)) {
    return mojom::TrustTokenOperationStatus::kBadResponse;
  }

  response->headers->RemoveHeader(kTrustTokensSecTrustTokenHeader);

  std::unique_ptr<Cryptographer::UnblindedTokens> maybe_tokens =
      cryptographer_->ConfirmIssuance(header_value);

  if (!maybe_tokens) {
    // The response was rejected by the underlying cryptographic library as
    // malformed or otherwise invalid.
    return mojom::TrustTokenOperationStatus::kBadResponse;
  }

  token_store_->AddTokens(issuer_, base::make_span(maybe_tokens->tokens),
                          maybe_tokens->body_of_verifying_key);

  return mojom::TrustTokenOperationStatus::kOk;
}

}  // namespace network
