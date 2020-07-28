// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_TOKENS_FRAME_TOKEN_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_TOKENS_FRAME_TOKEN_MOJOM_TRAITS_H_

#include "third_party/blink/public/common/tokens/frame_token.h"
#include "third_party/blink/public/common/tokens/token_mojom_traits_helper.h"
#include "third_party/blink/public/mojom/tokens/frame_token.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<blink::mojom::FrameTokenDataView, blink::FrameToken>
    : public blink::TokenMojomTraitsHelper<blink::mojom::FrameTokenDataView,
                                           blink::FrameToken> {};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_TOKENS_FRAME_TOKEN_MOJOM_TRAITS_H_
