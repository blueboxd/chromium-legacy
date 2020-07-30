// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_TOKENS_PORTAL_TOKEN_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_TOKENS_PORTAL_TOKEN_H_

#include "base/util/type_safety/token_type.h"

namespace blink {

// Typemapped to blink::mojom::PortalToken (see portal_token_mojom_traits.h).
using PortalToken = util::TokenType<class PortalTokenTypeMarker>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_TOKENS_PORTAL_TOKEN_H_
