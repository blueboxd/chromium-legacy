// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_TOKENS_FRAME_TOKEN_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_TOKENS_FRAME_TOKEN_H_

#include "base/util/type_safety/token_type.h"

namespace blink {

// Frame token type. Note that this token is type-mapped to its mojom equivalent
// (defined in third_party/blink/public/mojom/tokens/frame_token.mojom). See
// frame_token_mojom_traits.h for the StructTraits.
using FrameToken = util::TokenType<class FrameTokenTypeMarker>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_TOKENS_FRAME_TOKEN_H_
