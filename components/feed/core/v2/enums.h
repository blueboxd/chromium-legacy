// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_ENUMS_H_
#define COMPONENTS_FEED_CORE_V2_ENUMS_H_

#include <iosfwd>

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

enum class LoadStreamStatus {
  // Loading was not attempted.
  kNoStatus = 0,
  kLoadedFromStore = 1,
  kLoadedFromNetwork = 2,
  kFailedWithStoreError = 3,
  kNoStreamDataInStore = 4,
  kModelAlreadyLoaded = 5,
  kNoResponseBody = 6,
  // TODO(harringtond): Let's add more specific errors here.
  kProtoTranslationFailed = 7,
};

std::ostream& operator<<(std::ostream& out, LoadStreamStatus value);

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_ENUMS_H_
