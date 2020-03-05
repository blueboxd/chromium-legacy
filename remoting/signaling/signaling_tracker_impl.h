// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_SIGNALING_TRACKER_IMPL_H_
#define REMOTING_SIGNALING_SIGNALING_TRACKER_IMPL_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "remoting/signaling/signaling_tracker.h"

namespace remoting {

// Implementation of SignalingTracker.
class SignalingTrackerImpl final : public SignalingTracker {
 public:
  SignalingTrackerImpl();
  ~SignalingTrackerImpl() override;

  // SignalingTracker implementations.
  void OnChannelActive() override;
  bool IsChannelActive() const override;
  base::TimeDelta GetLastReportedChannelActiveDuration() const override;

 private:
  base::TimeTicks channel_last_active_time_;

  DISALLOW_COPY_AND_ASSIGN(SignalingTrackerImpl);
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_SIGNALING_TRACKER_IMPL_H_