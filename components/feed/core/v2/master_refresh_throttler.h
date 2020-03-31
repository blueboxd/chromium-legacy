// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_MASTER_REFRESH_THROTTLER_H_
#define COMPONENTS_FEED_CORE_V2_MASTER_REFRESH_THROTTLER_H_

#include "components/feed/core/common/enums.h"
#include "components/feed/core/common/refresh_throttler.h"
#include "components/feed/core/common/user_classifier.h"

class PrefService;

namespace feed {

// A refresh throttler that supports all |UserClass|es.
// TODO(crbug.com/1066230): When v2 is the only Feed implementation, make
// |RefreshThrottler| a private implementation detail of this class.
class MasterRefreshThrottler {
 public:
  MasterRefreshThrottler(PrefService* profile_prefs, const base::Clock* clock);
  MasterRefreshThrottler(const MasterRefreshThrottler&) = delete;
  MasterRefreshThrottler& operator=(const MasterRefreshThrottler&) = delete;

  bool RequestQuota(UserClass user_class);

 private:
  RefreshThrottler& GetThrottler(UserClass user_class);

  RefreshThrottler rare_throttler_;
  RefreshThrottler active_viewer_throttler_;
  RefreshThrottler active_consumer_throttler_;
};

}  // namespace feed
#endif  // COMPONENTS_FEED_CORE_V2_MASTER_REFRESH_THROTTLER_H_
