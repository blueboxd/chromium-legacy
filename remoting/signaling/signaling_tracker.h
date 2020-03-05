// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_SIGNALING_TRACKER_H_
#define REMOTING_SIGNALING_SIGNALING_TRACKER_H_

#include "base/macros.h"
#include "base/time/time.h"

namespace remoting {

// Interface to track signaling related information. Useful for telemetry and
// debugging.
class SignalingTracker {
 public:
  static constexpr base::TimeDelta kChannelInactiveTimeout =
      base::TimeDelta::FromMinutes(1);

  virtual ~SignalingTracker() = default;

  // Report that the channel is currently active.
  virtual void OnChannelActive() = 0;

  // Returns true if OnChannelActive() has been called after
  // now - kChannelInactiveTimeout.
  virtual bool IsChannelActive() const = 0;

  // Get the duration between (OnChannelActive() is called, now). Returns
  // base::TimeDelta::Max() if OnChannelActive() has never been called.
  virtual base::TimeDelta GetLastReportedChannelActiveDuration() const = 0;

 protected:
  SignalingTracker() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(SignalingTracker);
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_SIGNALING_TRACKER_H_
