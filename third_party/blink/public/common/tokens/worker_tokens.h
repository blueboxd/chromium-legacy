// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Definitions of all Blink token types. Each is of these has distinct mojo
// type maps for both Blink and non-Blink variants. See the various
// *_mojom_traits.h files in this directory.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_TOKENS_WORKER_TOKENS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_TOKENS_WORKER_TOKENS_H_

#include "base/util/type_safety/token_type.h"

namespace blink {

// Worker token types. Note that these token types are type-mapped to their
// mojom equivalents (defined in mojom/worker_tokens.mojom). See
// worker_tokens_mojom_traits for the StructTraits.
using DedicatedWorkerToken =
    util::TokenType<class DedicatedWorkerTokenTypeMarker>;
using SharedWorkerToken = util::TokenType<class SharedWorkerTokenTypeMarker>;
using ServiceWorkerToken = util::TokenType<class ServiceWorkerTokenTypeMarker>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_TOKENS_WORKER_TOKENS_H_
