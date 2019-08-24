// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/cast_external_begin_frame_client.h"

#include "base/time/time.h"
#include "chromecast/graphics/cast_window_manager_aura.h"

namespace chromecast {

CastExternalBeginFrameClient::CastExternalBeginFrameClient(
    CastWindowManagerAura* cast_window_manager_aura)
    : cast_window_manager_aura_(cast_window_manager_aura),
      sequence_number_(viz::BeginFrameArgs::kStartingFrameNumber),
      source_id_(viz::BeginFrameArgs::kManualSourceId),
      interval_(base::TimeDelta::FromMilliseconds(250)),
      frame_in_flight_(false) {}

CastExternalBeginFrameClient::~CastExternalBeginFrameClient() {}

void CastExternalBeginFrameClient::OnDisplayDidFinishFrame(
    const viz::BeginFrameAck& ack) {
  if (ack.source_id == source_id_)
    frame_in_flight_ = false;
}

void CastExternalBeginFrameClient::OnNeedsExternalBeginFrames(
    bool needs_begin_frames) {
  if (needs_begin_frames)
    timer_.Start(FROM_HERE, interval_, this,
                 &CastExternalBeginFrameClient::IssueExternalBeginFrame);
  else
    timer_.Stop();
}

void CastExternalBeginFrameClient::IssueExternalBeginFrame() {
  if (!frame_in_flight_) {
    viz::BeginFrameArgs args = viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, source_id_, sequence_number_,
        base::TimeTicks::Now(), base::TimeTicks::Now() + interval_, interval_,
        viz::BeginFrameArgs::NORMAL);
    ui::Compositor* compositor =
        cast_window_manager_aura_->window_tree_host()->compositor();
    compositor->context_factory_private()->IssueExternalBeginFrame(compositor,
                                                                   args);
    sequence_number_++;
    frame_in_flight_ = true;
  }
}

}  // namespace chromecast
