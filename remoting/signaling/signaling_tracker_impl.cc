// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/signaling_tracker_impl.h"

namespace remoting {

SignalingTrackerImpl::SignalingTrackerImpl() = default;

SignalingTrackerImpl::~SignalingTrackerImpl() = default;

void SignalingTrackerImpl::OnChannelActive() {
  channel_last_active_time_ = base::TimeTicks::Now();
}

bool SignalingTrackerImpl::IsChannelActive() const {
  return GetLastReportedChannelActiveDuration() <
         SignalingTracker::kChannelInactiveTimeout;
}

base::TimeDelta SignalingTrackerImpl::GetLastReportedChannelActiveDuration()
    const {
  if (channel_last_active_time_.is_null()) {
    return base::TimeDelta::Max();
  }
  return base::TimeTicks::Now() - channel_last_active_time_;
}

}  // namespace remoting
