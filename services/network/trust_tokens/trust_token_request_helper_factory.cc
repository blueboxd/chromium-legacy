// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_request_helper_factory.h"
#include <memory>

#include <utility>

#include "net/base/isolation_info.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "services/network/trust_tokens/ed25519_trust_token_request_signer.h"
#include "services/network/trust_tokens/suitable_trust_token_origin.h"
#include "services/network/trust_tokens/trust_token_http_headers.h"
#include "services/network/trust_tokens/trust_token_key_commitment_controller.h"
#include "services/network/trust_tokens/trust_token_request_canonicalizer.h"
#include "services/network/trust_tokens/trust_token_request_signing_helper.h"

namespace network {

TrustTokenRequestHelperFactory::TrustTokenRequestHelperFactory(
    PendingTrustTokenStore* store)
    : store_(store) {}
TrustTokenRequestHelperFactory::TrustTokenRequestHelperFactory() = default;
TrustTokenRequestHelperFactory::~TrustTokenRequestHelperFactory() = default;

void TrustTokenRequestHelperFactory::CreateTrustTokenHelperForRequest(
    const net::URLRequest& request,
    const mojom::TrustTokenParams& params,
    base::OnceCallback<void(TrustTokenStatusOrRequestHelper)> done) {
  for (base::StringPiece header : TrustTokensRequestHeaders()) {
    if (request.extra_request_headers().HasHeader(header)) {
      std::move(done).Run(mojom::TrustTokenOperationStatus::kInvalidArgument);
      return;
    }
  }

  base::Optional<SuitableTrustTokenOrigin> maybe_top_frame_origin;
  if (request.isolation_info().top_frame_origin()) {
    maybe_top_frame_origin = SuitableTrustTokenOrigin::Create(
        *request.isolation_info().top_frame_origin());
  }
  if (!maybe_top_frame_origin) {
    std::move(done).Run(mojom::TrustTokenOperationStatus::kFailedPrecondition);
    return;
  }

  store_->ExecuteOrEnqueue(
      base::BindOnce(&TrustTokenRequestHelperFactory::ConstructHelperUsingStore,
                     weak_factory_.GetWeakPtr(), *maybe_top_frame_origin,
                     base::Passed(params.Clone()), std::move(done)));
}

void TrustTokenRequestHelperFactory::ConstructHelperUsingStore(
    SuitableTrustTokenOrigin top_frame_origin,
    mojom::TrustTokenParamsPtr params,
    base::OnceCallback<void(TrustTokenStatusOrRequestHelper)> done,
    TrustTokenStore* store) {
  DCHECK(params);

  switch (params->type) {
    case mojom::TrustTokenOperationType::kSigning: {
      base::Optional<SuitableTrustTokenOrigin> maybe_issuer;
      if (params->issuer) {
        maybe_issuer =
            SuitableTrustTokenOrigin::Create(std::move(*params->issuer));
      }

      if (!maybe_issuer) {
        std::move(done).Run(mojom::TrustTokenOperationStatus::kInvalidArgument);
        return;
      }

      TrustTokenRequestSigningHelper::Params signing_params(
          std::move(*maybe_issuer), top_frame_origin,
          std::move(params->additional_signed_headers),
          params->include_timestamp_header, params->sign_request_data);

      std::move(done).Run(std::unique_ptr<TrustTokenRequestHelper>(
          new TrustTokenRequestSigningHelper(
              store, std::move(signing_params),
              std::make_unique<Ed25519TrustTokenRequestSigner>(),
              std::make_unique<TrustTokenRequestCanonicalizer>())));
      return;
    }

    // Issuance and redemption aren't yet implemented.
    case mojom::TrustTokenOperationType::kIssuance:
    case mojom::TrustTokenOperationType::kRedemption: {
      std::move(done).Run(mojom::TrustTokenOperationStatus::kUnavailable);
      return;
    }
  }
}

TrustTokenStatusOrRequestHelper::TrustTokenStatusOrRequestHelper() = default;

TrustTokenStatusOrRequestHelper::TrustTokenStatusOrRequestHelper(
    mojom::TrustTokenOperationStatus status)
    : status_(status) {
  DCHECK_NE(status_, mojom::TrustTokenOperationStatus::kOk);
}
TrustTokenStatusOrRequestHelper::TrustTokenStatusOrRequestHelper(
    std::unique_ptr<TrustTokenRequestHelper> helper)
    : status_(mojom::TrustTokenOperationStatus::kOk),
      helper_(std::move(helper)) {
  DCHECK(helper_);
}

TrustTokenStatusOrRequestHelper::~TrustTokenStatusOrRequestHelper() = default;

TrustTokenStatusOrRequestHelper::TrustTokenStatusOrRequestHelper(
    TrustTokenStatusOrRequestHelper&&) = default;
TrustTokenStatusOrRequestHelper& TrustTokenStatusOrRequestHelper::operator=(
    TrustTokenStatusOrRequestHelper&&) = default;

}  // namespace network
