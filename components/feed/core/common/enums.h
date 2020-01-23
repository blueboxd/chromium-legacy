// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_COMMON_ENUMS_H_
#define COMPONENTS_FEED_CORE_COMMON_ENUMS_H_

// This file contains enumerations common to Feed v1 and v2.

namespace feed {

// The TriggerType enum specifies values for the events that can trigger
// refreshing articles. When adding values, be certain to also update the
// corresponding definition in enums.xml.
enum class TriggerType {
  kNtpShown = 0,
  kForegrounded = 1,
  kFixedTimer = 2,
  kMaxValue = kFixedTimer
};

// Different groupings of usage. A user will belong to exactly one of these at
// any given point in time. Can change at runtime.
enum class UserClass {
  kRareSuggestionsViewer,      // Almost never opens surfaces that show
                               // suggestions, like the NTP.
  kActiveSuggestionsViewer,    // Frequently shown suggestions, but does not
                               // usually open them.
  kActiveSuggestionsConsumer,  // Frequently opens news articles.
};

// Enum for the status of the refresh, reported through UMA.
// If any new values are added, update FeedSchedulerRefreshStatus in
// enums.xml.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ShouldRefreshResult {
  kShouldRefresh = 0,
  kDontRefreshOutstandingRequest = 1,
  kDontRefreshTriggerDisabled = 2,
  kDontRefreshNetworkOffline = 3,
  kDontRefreshEulaNotAccepted = 4,
  kDontRefreshArticlesHidden = 5,
  kDontRefreshRefreshSuppressed = 6,
  kDontRefreshNotStale = 7,
  kDontRefreshRefreshThrottled = 8,
  kMaxValue = kDontRefreshRefreshThrottled,
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_COMMON_ENUMS_H_
