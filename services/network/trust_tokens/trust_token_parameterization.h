// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_PARAMETERIZATION_H_
#define SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_PARAMETERIZATION_H_

#include "base/component_export.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"

namespace network {

// Priority for running blocking Trust Tokens database IO. This is given type
// USER_VISIBLE because Trust Tokens DB operations can sometimes be in the
// loading critical path, but generally only for subresources.
extern const base::TaskPriority kTrustTokenDatabaseTaskPriority;

// The maximum time Trust Tokens backing database writes will be buffered before
// being committed to disk. Two seconds was chosen fairly arbitrarily as a value
// close to what the cookie store uses.
extern const base::TimeDelta kTrustTokenWriteBufferingWindow;

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_PARAMETERIZATION_H_
