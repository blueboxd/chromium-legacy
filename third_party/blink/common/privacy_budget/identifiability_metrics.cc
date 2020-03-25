// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/privacy_budget/identifiability_metrics.h"

#include <cstdint>

#include "base/hash/hash.h"
#include "base/logging.h"

namespace blink {

uint64_t DigestForMetrics(base::span<const uint8_t> in) {
  // NOTE: As documented at the point of declaration, the digest calculated here
  // should be stable once released.
  return base::PersistentHash(in);
}

}  // namespace blink
