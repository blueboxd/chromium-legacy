// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/throughput_tracker.h"

#include <utility>

#include "base/callback.h"
#include "base/check.h"

namespace ui {

ThroughputTracker::ThroughputTracker(TrackerId id, ThroughputTrackerHost* host)
    : id_(id), host_(host) {
  DCHECK(host_);
}

ThroughputTracker::ThroughputTracker(ThroughputTracker&& other) {
  *this = std::move(other);
}

ThroughputTracker& ThroughputTracker::operator=(ThroughputTracker&& other) {
  id_ = other.id_;
  host_ = other.host_;
  started_ = other.started_;

  other.id_ = kInvalidId;
  other.host_ = nullptr;
  other.started_ = false;
  return *this;
}

ThroughputTracker::~ThroughputTracker() {
  if (started_)
    Cancel();
}

void ThroughputTracker::Start(ThroughputTrackerHost::ReportCallback callback) {
  started_ = true;
  host_->StartThroughputTracker(id_, std::move(callback));
}

void ThroughputTracker::Stop() {
  started_ = false;
  host_->StopThroughtputTracker(id_);
}

void ThroughputTracker::Cancel() {
  started_ = false;
  host_->CancelThroughtputTracker(id_);
}

}  // namespace ui
