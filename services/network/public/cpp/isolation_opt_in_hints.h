// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_ISOLATION_OPT_IN_HINTS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_ISOLATION_OPT_IN_HINTS_H_

#include <string>

#include "base/component_export.h"

namespace network {

// The following definitions correspond to the draft spec found here:
// https://github.com/domenic/origin-isolation#proposed-hints.
enum class IsolationOptInHints : unsigned {
  NO_HINTS = 0x0,
  PREFER_ISOLATED_EVENT_LOOP = 0x1,
  PREFER_ISOLATED_MEMORY = 0x2,
  FOR_SIDE_CHANNEL_PROTECTION = 0x4,
  FOR_MEMORY_ISOLATION = 0x8,
  ALL_HINTS_ACTIVE = 0xF
};

// Converts hint strings into their corresponding IsolationOptInHints values.
// The hint strings are specified at
// https://github.com/domenic/origin-isolation#proposed-hints.
COMPONENT_EXPORT(NETWORK_CPP_BASE)
IsolationOptInHints GetIsolationOptInHintFromString(
    const std::string& hint_str);

COMPONENT_EXPORT(NETWORK_CPP_BASE)
IsolationOptInHints& operator|=(IsolationOptInHints& lhs,
                                IsolationOptInHints& rhs);

COMPONENT_EXPORT(NETWORK_CPP_BASE)
IsolationOptInHints& operator&(IsolationOptInHints& lhs,
                               IsolationOptInHints& rhs);

}  // namespace network
#endif  // SERVICES_NETWORK_PUBLIC_CPP_ISOLATION_OPT_IN_HINTS_H_
