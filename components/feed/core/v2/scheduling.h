// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_SCHEDULING_H_
#define COMPONENTS_FEED_CORE_V2_SCHEDULING_H_

#include "base/time/time.h"
#include "components/feed/core/v2/enums.h"

namespace feed {
constexpr base::TimeDelta kSuppressRefreshDuration =
    base::TimeDelta::FromMinutes(30);

// Returns a duration, T, depending on the UserClass and TriggerType.
// The following should be true:
// - At most one fetch is attempted per T.
// - Content is considered stale if time since last fetch is > T. We'll prefer
//   to refresh stale content before showing it.
// - For TriggerType::kFixedTimer, T is the time between scheduled fetches.
base::TimeDelta GetUserClassTriggerThreshold(UserClass user_class,
                                             TriggerType trigger);

// Returns whether we should wait for new content before showing stream content.
bool ShouldWaitForNewContent(UserClass user_class,
                             bool has_content,
                             base::TimeDelta content_age);
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_SCHEDULING_H_
