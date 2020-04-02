// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_PREFS_H_
#define COMPONENTS_FEED_CORE_V2_PREFS_H_

#include <vector>

#include "base/time/time.h"

class PrefService;

namespace feed {
namespace prefs {

// Functions for accessing prefs.

// For counting previously made requests, one integer for each
// |NetworkRequestType|.
std::vector<int> GetThrottlerRequestCounts(PrefService* pref_service);
void SetThrottlerRequestCounts(std::vector<int> request_counts,
                               PrefService* pref_service);

// Time of the last request. For determining whether the next day's quota should
// be released.
base::Time GetLastRequestTime(PrefService* pref_service);
void SetLastRequestTime(base::Time request_time, PrefService* pref_service);

}  // namespace prefs
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_PREFS_H_
