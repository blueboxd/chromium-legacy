// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_TOKENS_TOKENS_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_TOKENS_TOKENS_MOJOM_TRAITS_H_

#include "third_party/blink/public/common/tokens/token_mojom_traits_helper.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/tokens/tokens.mojom-shared.h"

namespace mojo {

// Mojom traits for the various token types.
// See third_party/blink/public/common/tokens/tokens.h for more details.

////////////////////////////////////////////////////////////////////////////////
// FRAME TOKENS

template <>
struct StructTraits<blink::mojom::LocalFrameTokenDataView,
                    blink::LocalFrameToken>
    : public blink::TokenMojomTraitsHelper<
          blink::mojom::LocalFrameTokenDataView,
          blink::LocalFrameToken> {};

template <>
struct StructTraits<blink::mojom::RemoteFrameTokenDataView,
                    blink::RemoteFrameToken>
    : public blink::TokenMojomTraitsHelper<
          blink::mojom::RemoteFrameTokenDataView,
          blink::RemoteFrameToken> {};

////////////////////////////////////////////////////////////////////////////////
// WORKER TOKENS

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

////////////////////////////////////////////////////////////////////////////////
// OTHER TOKENS
//
// Keep this section last.
//
// If you have multiple tokens that make a thematic group, please lift them to
// their own section, in alphabetical order. If adding a new token here, please
// keep the following list in alphabetic order.

template <>
struct StructTraits<blink::mojom::PortalTokenDataView, blink::PortalToken>
    : public blink::TokenMojomTraitsHelper<blink::mojom::PortalTokenDataView,
                                           blink::PortalToken> {};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_TOKENS_TOKENS_MOJOM_TRAITS_H_
