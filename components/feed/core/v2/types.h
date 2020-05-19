// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_TYPES_H_
#define COMPONENTS_FEED_CORE_V2_TYPES_H_

#include <string>

#include "base/util/type_safety/id_type.h"
#include "base/values.h"
#include "components/feed/core/v2/public/types.h"

namespace feed {

// Make sure public types are included here too.
// See components/feed/core/v2/public/types.h.
using ::feed::ChromeInfo;
using ::feed::EphemeralChangeId;

// Uniquely identifies a revision of a |feedstore::Content|. If Content changes,
// it is assigned a new revision number.
using ContentRevision = util::IdTypeU32<class ContentRevisionClass>;

// ID for a stored pending action.
using LocalActionId = util::IdType32<class LocalActionIdClass>;

std::string ToString(ContentRevision c);
ContentRevision ToContentRevision(const std::string& str);

// Metadata sent with Feed requests.
struct RequestMetadata {
  ChromeInfo chrome_info;
  std::string language_tag;
  DisplayMetrics display_metrics;
};

// Data internal to MetricsReporter which is persisted to Prefs.
struct PersistentMetricsData {
  // The midnight time for the day in which this metric was recorded.
  base::Time current_day_start;
  // The total recorded time spent on the Feed for the current day.
  base::TimeDelta accumulated_time_spent_in_feed;
};

base::Value PersistentMetricsDataToValue(const PersistentMetricsData& data);
PersistentMetricsData PersistentMetricsDataFromValue(const base::Value& value);

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_TYPES_H_
