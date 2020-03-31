// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_ENUMS_H_
#define COMPONENTS_FEED_CORE_V2_ENUMS_H_

#include "components/feed/core/common/enums.h"

namespace feed {

// Describes the behavior for attempting to refresh (over the network) while
// loading the feed.
enum class LoadRefreshBehavior {
  // Wait for feed refresh before showing the result.
  kWaitForRefresh,
  // Load what is available locally, begin the refresh, and populate results
  // below the fold when they are received.
  kRefreshInline,
  // Wait a limited amount of time for the network fetch. If the fetch doesn't
  // complete in time, just show the user what's available locally.
  kLimitedWaitForRefresh,
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_ENUMS_H_
