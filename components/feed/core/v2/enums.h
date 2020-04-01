// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_ENUMS_H_
#define COMPONENTS_FEED_CORE_V2_ENUMS_H_

#include <iosfwd>

#include "components/feed/core/common/enums.h"

namespace feed {

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
  kDataInStoreIsStale = 8,
  // The timestamp for stored data is in the future, so we're treating stored
  // data as it it is stale.
  kDataInStoreIsStaleTimestampInFuture = 9,
};

std::ostream& operator<<(std::ostream& out, LoadStreamStatus value);

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_ENUMS_H_
