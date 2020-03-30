// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_request_redemption_helper.h"

#include <algorithm>
#include <utility>

#include "base/callback.h"
#include "base/stl_util.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/trust_tokens/proto/public.pb.h"
#include "services/network/trust_tokens/trust_token_database_owner.h"
#include "services/network/trust_tokens/trust_token_http_headers.h"
#include "services/network/trust_tokens/trust_token_parameterization.h"
#include "services/network/trust_tokens/trust_token_store.h"
#include "url/url_constants.h"

namespace network {

TrustTokenRequestRedemptionHelper::TrustTokenRequestRedemptionHelper(
    const url::Origin& top_level_origin,
    mojom::TrustTokenRefreshPolicy refresh_policy,
    TrustTokenStore* token_store,
    std::unique_ptr<TrustTokenKeyCommitmentGetter> key_commitment_getter,
    std::unique_ptr<KeyPairGenerator> key_pair_generator,
    std::unique_ptr<Cryptographer> cryptographer)
    : top_level_origin_(top_level_origin),
      refresh_policy_(refresh_policy),
      token_store_(token_store),
      key_commitment_getter_(std::move(key_commitment_getter)),
      key_pair_generator_(std::move(key_pair_generator)),
      cryptographer_(std::move(cryptographer)) {
  DCHECK(top_level_origin.scheme() == url::kHttpsScheme ||
         (top_level_origin.scheme() == url::kHttpScheme &&
          IsOriginPotentiallyTrustworthy(top_level_origin)))
      << top_level_origin;

  DCHECK(token_store_);
  DCHECK(key_commitment_getter_);
  DCHECK(key_pair_generator_);
  DCHECK(cryptographer_);
}

TrustTokenRequestRedemptionHelper::~TrustTokenRequestRedemptionHelper() =
    default;

void TrustTokenRequestRedemptionHelper::Begin(
    net::URLRequest* request,
    base::OnceCallback<void(mojom::TrustTokenOperationStatus)> done) {
  DCHECK(request->url().SchemeIsHTTPOrHTTPS() &&
         IsUrlPotentiallyTrustworthy(request->url()))
      << request->url();
  DCHECK(request->initiator() &&
             request->initiator()->scheme() == url::kHttpsScheme ||
         (request->initiator()->scheme() == url::kHttpScheme &&
          IsOriginPotentiallyTrustworthy(*request->initiator())))
      << (request->initiator() ? request->initiator()->Serialize() : "(none)");

  issuer_ = url::Origin::Create(request->url());

  if (refresh_policy_ == mojom::TrustTokenRefreshPolicy::kRefresh &&
      !request->initiator()->IsSameOriginWith(issuer_)) {
    std::move(done).Run(mojom::TrustTokenOperationStatus::kFailedPrecondition);
    return;
  }

  if (!token_store_->SetAssociation(issuer_, top_level_origin_)) {
    std::move(done).Run(mojom::TrustTokenOperationStatus::kResourceExhausted);
    return;
  }

  if (refresh_policy_ == mojom::TrustTokenRefreshPolicy::kUseCached &&
      token_store_->RetrieveNonstaleRedemptionRecord(issuer_,
                                                     top_level_origin_)) {
    std::move(done).Run(mojom::TrustTokenOperationStatus::kAlreadyExists);
    return;
  }

  key_commitment_getter_->Get(
      issuer_,
      base::BindOnce(&TrustTokenRequestRedemptionHelper::OnGotKeyCommitment,
                     weak_factory_.GetWeakPtr(), request, std::move(done)));
}

void TrustTokenRequestRedemptionHelper::OnGotKeyCommitment(
    net::URLRequest* request,
    base::OnceCallback<void(mojom::TrustTokenOperationStatus)> done,
    mojom::TrustTokenKeyCommitmentResultPtr commitment_result) {
  if (!commitment_result) {
    std::move(done).Run(mojom::TrustTokenOperationStatus::kFailedPrecondition);
    return;
  }

  // Evict tokens signed with keys other than those from the issuer's most
  // recent commitments.
  token_store_->PruneStaleIssuerState(issuer_, commitment_result->keys);

  base::Optional<TrustToken> maybe_token_to_redeem = RetrieveSingleToken();
  if (!maybe_token_to_redeem) {
    std::move(done).Run(mojom::TrustTokenOperationStatus::kResourceExhausted);
    return;
  }

  if (!key_pair_generator_->Generate(&signing_key_, &verification_key_)) {
    std::move(done).Run(mojom::TrustTokenOperationStatus::kInternalError);
    return;
  }

  base::Optional<std::string> maybe_redemption_header =
      cryptographer_->BeginRedemption(*maybe_token_to_redeem, verification_key_,
                                      top_level_origin_);

  if (!maybe_redemption_header) {
    std::move(done).Run(mojom::TrustTokenOperationStatus::kInternalError);
    return;
  }

  request->SetExtraRequestHeaderByName(kTrustTokensSecTrustTokenHeader,
                                       std::move(*maybe_redemption_header),
                                       /*overwrite=*/true);

  // We don't want cache reads, because the highest priority is to execute the
  // protocol operation by sending the server the Trust Tokens request header
  // and getting the corresponding response header, but we want cache writes
  // in case subsequent requests are made to the same URL in non-trust-token
  // settings.
  request->SetLoadFlags(request->load_flags() | net::LOAD_BYPASS_CACHE);

  token_store_->DeleteToken(issuer_, *maybe_token_to_redeem);

  std::move(done).Run(mojom::TrustTokenOperationStatus::kOk);
}

mojom::TrustTokenOperationStatus TrustTokenRequestRedemptionHelper::Finalize(
    mojom::URLResponseHead* response) {
  // Numbers 1-4 below correspond to the lines of the "Process a redemption
  // response" pseudocode from the design doc.

  DCHECK(response);

  // A response headers object should be present on all responses for
  // HTTP requests (which Trust Tokens requests are).
  DCHECK(response->headers);

  // 1. If the response has no Sec-Trust-Token header, return an error.

  std::string header_value;

  // EnumerateHeader(|iter|=nullptr) asks for the first instance of the header,
  // if any.
  if (!response->headers->EnumerateHeader(
          /*iter=*/nullptr, kTrustTokensSecTrustTokenHeader, &header_value)) {
    return mojom::TrustTokenOperationStatus::kBadResponse;
  }

  // 2. Strip the Sec-Trust-Token header, from the response and pass the header,
  // base64-decoded, to BoringSSL, along with the issuer’s SRR-verification
  // public key previously obtained from a key commitment.
  response->headers->RemoveHeader(kTrustTokensSecTrustTokenHeader);

  base::Optional<std::string> maybe_signed_redemption_record =
      cryptographer_->ConfirmRedemption(header_value);

  // 3. If BoringSSL fails its structural validation / signature check, return
  // an error.
  if (!maybe_signed_redemption_record) {
    // The response was rejected by the underlying cryptographic library as
    // malformed or otherwise invalid.
    return mojom::TrustTokenOperationStatus::kBadResponse;
  }

  // 4. Otherwise, if these checks succeed, store the SRR and return success.

  SignedTrustTokenRedemptionRecord record_to_store;
  record_to_store.set_body(std::move(*maybe_signed_redemption_record));
  record_to_store.set_signing_key(std::move(signing_key_));
  record_to_store.set_public_key(std::move(verification_key_));
  token_store_->SetRedemptionRecord(issuer_, top_level_origin_,
                                    std::move(record_to_store));

  return mojom::TrustTokenOperationStatus::kOk;
}

base::Optional<TrustToken>
TrustTokenRequestRedemptionHelper::RetrieveSingleToken() {
  // As a postcondition of UpdateTokenStoreFromKeyCommitmentResult, all of the
  // store's tokens for |issuer_| match the key commitment result obtained at
  // the beginning of this redemption. Consequently, it's OK to use any
  // |issuer_| token in the store.
  auto key_matcher =
      base::BindRepeating([](const std::string&) { return true; });

  std::vector<TrustToken> matching_tokens =
      token_store_->RetrieveMatchingTokens(issuer_, key_matcher);

  if (matching_tokens.empty())
    return base::nullopt;

  return matching_tokens.front();
}

}  // namespace network
