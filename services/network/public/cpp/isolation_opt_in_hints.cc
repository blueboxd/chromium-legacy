// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/isolation_opt_in_hints.h"

#include <type_traits>

namespace network {

IsolationOptInHints GetIsolationOptInHintFromString(
    const std::string& hint_str) {
  if (hint_str == "prefer_isolated_event_loop")
    return IsolationOptInHints::PREFER_ISOLATED_EVENT_LOOP;
  if (hint_str == "prefer_isolated_memory")
    return IsolationOptInHints::PREFER_ISOLATED_MEMORY;
  if (hint_str == "for_side_channel_protection")
    return IsolationOptInHints::FOR_SIDE_CHANNEL_PROTECTION;
  if (hint_str == "for_memory_measurement")
    return IsolationOptInHints::FOR_MEMORY_ISOLATION;
  return IsolationOptInHints::NO_HINTS;
}

IsolationOptInHints& operator|=(IsolationOptInHints& lhs,
                                IsolationOptInHints& rhs) {
  lhs = static_cast<IsolationOptInHints>(
      static_cast<std::underlying_type<IsolationOptInHints>::type>(lhs) |
      static_cast<std::underlying_type<IsolationOptInHints>::type>(rhs));
  return lhs;
}

IsolationOptInHints& operator&(IsolationOptInHints& lhs,
                               IsolationOptInHints& rhs) {
  lhs = static_cast<IsolationOptInHints>(
      static_cast<std::underlying_type<IsolationOptInHints>::type>(lhs) &
      static_cast<std::underlying_type<IsolationOptInHints>::type>(rhs));
  return lhs;
}

}  // namespace network
