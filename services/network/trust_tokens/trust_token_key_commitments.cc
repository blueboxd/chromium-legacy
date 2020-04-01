// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_key_commitments.h"

#include <utility>

#include "base/optional.h"
#include "services/network/public/mojom/trust_tokens.mojom-forward.h"
#include "services/network/trust_tokens/suitable_trust_token_origin.h"

namespace network {

TrustTokenKeyCommitments::TrustTokenKeyCommitments() = default;
TrustTokenKeyCommitments::~TrustTokenKeyCommitments() = default;

void TrustTokenKeyCommitments::Set(
    base::flat_map<url::Origin, mojom::TrustTokenKeyCommitmentResultPtr> map) {
  // To filter out the unsuitable origins in linear time, extract |map|'s
  // contents a vector, filter the vector, and place the result back into
  // |map_|.

  std::vector<std::pair<url::Origin, mojom::TrustTokenKeyCommitmentResultPtr>>
      to_filter(std::move(map).extract());

  std::vector<std::pair<SuitableTrustTokenOrigin,
                        mojom::TrustTokenKeyCommitmentResultPtr>>
      filtered;

  // Due to the characteristics of the Trust Tokens protocol, it is expected
  // that there be no more than a couple hundred issuer origins.
  for (std::pair<url::Origin, mojom::TrustTokenKeyCommitmentResultPtr>& kv :
       to_filter) {
    auto maybe_suitable_origin =
        SuitableTrustTokenOrigin::Create(std::move(kv.first));
    if (!maybe_suitable_origin)
      continue;
    filtered.emplace_back(std::move(*maybe_suitable_origin),
                          std::move(kv.second));
  }

  map_.replace(std::move(filtered));
}

void TrustTokenKeyCommitments::Get(
    const url::Origin& origin,
    base::OnceCallback<void(mojom::TrustTokenKeyCommitmentResultPtr)> done)
    const {
  base::Optional<SuitableTrustTokenOrigin> suitable_origin =
      SuitableTrustTokenOrigin::Create(origin);
  if (!suitable_origin) {
    std::move(done).Run(nullptr);
    return;
  }

  auto it = map_.find(*suitable_origin);
  if (it == map_.end()) {
    std::move(done).Run(nullptr);
    return;
  }

  std::move(done).Run(it->second->Clone());
}

}  // namespace network
