// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_TOKENS_WORKER_TOKENS_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_TOKENS_WORKER_TOKENS_MOJOM_TRAITS_H_

#include "third_party/blink/public/common/tokens/token_mojom_traits_helper.h"
#include "third_party/blink/public/common/tokens/worker_tokens.h"
#include "third_party/blink/public/mojom/tokens/worker_tokens.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<blink::mojom::DedicatedWorkerTokenDataView,
                    blink::DedicatedWorkerToken>
    : public blink::TokenMojomTraitsHelper<
          blink::mojom::DedicatedWorkerTokenDataView,
          blink::DedicatedWorkerToken> {};

template <>
struct StructTraits<blink::mojom::ServiceWorkerTokenDataView,
                    blink::ServiceWorkerToken>
    : public blink::TokenMojomTraitsHelper<
          blink::mojom::ServiceWorkerTokenDataView,
          blink::ServiceWorkerToken> {};

template <>
struct StructTraits<blink::mojom::SharedWorkerTokenDataView,
                    blink::SharedWorkerToken>
    : public blink::TokenMojomTraitsHelper<
          blink::mojom::SharedWorkerTokenDataView,
          blink::SharedWorkerToken> {};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_TOKENS_WORKER_TOKENS_MOJOM_TRAITS_H_
