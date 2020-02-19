// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_PARAMETERIZATION_H_
#define SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_PARAMETERIZATION_H_

#include "base/component_export.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"

namespace network {

// Priority for running blocking Trust Tokens database IO. This is given value
// USER_VISIBLE because Trust Tokens DB operations can sometimes be in the
// loading critical path, but generally only for subresources.
constexpr base::TaskPriority kTrustTokenDatabaseTaskPriority =
    base::TaskPriority::USER_VISIBLE;

// The maximum time Trust Tokens backing database writes will be buffered before
// being committed to disk. Two seconds was chosen fairly arbitrarily as a value
// close to what the cookie store uses.
constexpr base::TimeDelta kTrustTokenWriteBufferingWindow =
    base::TimeDelta::FromSeconds(2);

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_PARAMETERIZATION_H_
