// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/master_refresh_throttler.h"

#include "components/prefs/pref_service.h"

namespace feed {

MasterRefreshThrottler::MasterRefreshThrottler(PrefService* profile_prefs,
                                               const base::Clock* clock)
    : rare_throttler_(UserClass::kRareSuggestionsViewer, profile_prefs, clock),
      active_viewer_throttler_(UserClass::kActiveSuggestionsViewer,
                               profile_prefs,
                               clock),
      active_consumer_throttler_(UserClass::kActiveSuggestionsConsumer,
                                 profile_prefs,
                                 clock)

{}

bool MasterRefreshThrottler::RequestQuota(UserClass user_class) {
  return GetThrottler(user_class).RequestQuota();
}

RefreshThrottler& MasterRefreshThrottler::GetThrottler(UserClass user_class) {
  switch (user_class) {
    case UserClass::kRareSuggestionsViewer:
      return rare_throttler_;
    case UserClass::kActiveSuggestionsViewer:
      return active_viewer_throttler_;
    case UserClass::kActiveSuggestionsConsumer:
      return active_consumer_throttler_;
  }
}

}  // namespace feed
