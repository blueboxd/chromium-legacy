// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_LOADER_UTIL_H_
#define SERVICES_NETWORK_LOADER_UTIL_H_

#include "base/component_export.h"

namespace network {

// Accept header used for frame requests.
COMPONENT_EXPORT(NETWORK_SERVICE)
extern const char kFrameAcceptHeader[];

// The default Accept header value to use if none were specified.
COMPONENT_EXPORT(NETWORK_SERVICE)
extern const char kDefaultAcceptHeader[];

}  // namespace network

#endif  // SERVICES_NETWORK_LOADER_UTIL_H_
