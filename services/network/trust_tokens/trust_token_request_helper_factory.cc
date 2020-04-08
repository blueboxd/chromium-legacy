// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_request_helper_factory.h"

#include <utility>

#include "net/base/isolation_info.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "services/network/trust_tokens/suitable_trust_token_origin.h"
#include "services/network/trust_tokens/trust_token_http_headers.h"

namespace network {

TrustTokenRequestHelperFactory::TrustTokenRequestHelperFactory(
    PendingTrustTokenStore* store)
    : store_(store) {}

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

  // Silence the compile warning: |store_| will be used to construct concrete
  // TrustTokenRequestHelpers once they are implemented.
  ignore_result(store_);

  // (Currently, there's no asynchronous logic here; subsequent CLs will change
  // this, since actually creating the helpers will require using the
  // TrustTokenStore into which |store_| materializes asynchronously.)

  std::move(done).Run(mojom::TrustTokenOperationStatus::kUnavailable);
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
