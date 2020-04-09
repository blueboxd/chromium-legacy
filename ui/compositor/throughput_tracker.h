// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_THROUGHPUT_TRACKER_H_
#define UI_COMPOSITOR_THROUGHPUT_TRACKER_H_

#include "base/callback_forward.h"
#include "ui/compositor/compositor_export.h"
#include "ui/compositor/throughput_tracker_host.h"

namespace ui {

class Compositor;
class ThroughputTrackerHost;

// A class to track the throughput of Compositor. The tracking is identified by
// an id. The id is passed into impl side and be used as the sequence id to
// create and stop a kCustom typed cc::FrameSequenceTracker. The class is
// move-only to have only one holder of the id. When ThroughputTracker is
// destroyed with an active tracking, the tracking will be canceled and report
// callback will not be invoked.
class COMPOSITOR_EXPORT ThroughputTracker {
 public:
  using TrackerId = ThroughputTrackerHost::TrackerId;

  // Move only.
  ThroughputTracker(ThroughputTracker&& other);
  ThroughputTracker& operator=(ThroughputTracker&& other);

  ~ThroughputTracker();

  // Starts tracking Compositor and provides a callback for reporting. The
  // throughput data collection starts after the next commit.
  void Start(ThroughputTrackerHost::ReportCallback callback);

  // Stops tracking. The supplied callback will be invoked when the data
  // collection finishes after the next frame presentation. Note that no data
  // will be reported if Stop() is not called,
  void Stop();

  // Cancels tracking. The supplied callback will not be invoked.
  void Cancel();

 private:
  friend class Compositor;

  // Private since it should only be created via Compositor's
  // RequestNewThroughputTracker call.
  ThroughputTracker(TrackerId id, ThroughputTrackerHost* host);

  static const TrackerId kInvalidId = 0u;
  TrackerId id_ = kInvalidId;
  ThroughputTrackerHost* host_ = nullptr;
  bool started_ = false;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_THROUGHPUT_TRACKER_H_
