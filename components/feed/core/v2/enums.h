// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_ENUMS_H_
#define COMPONENTS_FEED_CORE_V2_ENUMS_H_

#include <iosfwd>

#include "components/feed/core/common/enums.h"

namespace feed {

enum class NetworkRequestType : int {
  kFeedQuery = 0,
  kUploadActions = 1,
};

// This must be kept in sync with FeedLoadStreamStatus in enums.xml.
enum class LoadStreamStatus {
  // Loading was not attempted.
  kNoStatus = 0,
  kLoadedFromStore = 1,
  kLoadedFromNetwork = 2,
  kFailedWithStoreError = 3,
  kNoStreamDataInStore = 4,
  kModelAlreadyLoaded = 5,
  kNoResponseBody = 6,
  // TODO(harringtond): Let's add more specific proto translation errors.
  kProtoTranslationFailed = 7,
  kDataInStoreIsStale = 8,
  // The timestamp for stored data is in the future, so we're treating stored
  // data as it it is stale.
  kDataInStoreIsStaleTimestampInFuture = 9,
  kCannotLoadFromNetworkSupressedForHistoryDelete = 10,
  kCannotLoadFromNetworkOffline = 11,
  kCannotLoadFromNetworkThrottled = 12,
  kLoadNotAllowedEulaNotAccepted = 13,
  kLoadNotAllowedArticlesListHidden = 14,
  // TODO(harringtond): Emit this status value.
  kCannotParseNetorkResponseBody = 15,
  kLoadMoreModelIsNotLoaded = 16,
  kMaxValue = kLoadMoreModelIsNotLoaded,
};

std::ostream& operator<<(std::ostream& out, LoadStreamStatus value);

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_ENUMS_H_
