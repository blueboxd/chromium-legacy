// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_PROXY_COMMON_H_
#define CC_TREES_PROXY_COMMON_H_

#include <stddef.h>

#include "base/callback_forward.h"
#include "cc/cc_export.h"
#include "cc/metrics/frame_sequence_tracker_collection.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"

namespace cc {

struct ScrollAndScaleSet;
class MutatorEvents;

struct CC_EXPORT BeginMainFrameAndCommitState {
  BeginMainFrameAndCommitState();
  ~BeginMainFrameAndCommitState();

  unsigned int begin_frame_id = 0;
  viz::BeginFrameArgs begin_frame_args;
  std::unique_ptr<ScrollAndScaleSet> scroll_info;
  size_t memory_allocation_limit_bytes = 0;
  std::vector<std::pair<int, bool>> completed_image_decode_requests;
  std::unique_ptr<MutatorEvents> mutator_events;
  // Bit encoding of the FrameSequenceTrackerType for active trackers
  ActiveFrameSequenceTrackers active_sequence_trackers = 0;
  bool evicted_ui_resources = false;
};

}  // namespace cc

#endif  // CC_TREES_PROXY_COMMON_H_
