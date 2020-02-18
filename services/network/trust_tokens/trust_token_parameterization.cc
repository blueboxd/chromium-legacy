// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_parameterization.h"

namespace network {

constexpr base::TaskPriority kTrustTokenDatabaseTaskPriority =
    base::TaskPriority::USER_VISIBLE;

constexpr base::TimeDelta kTrustTokenWriteBufferingWindow =
    base::TimeDelta::FromSeconds(2);

}  // namespace network
