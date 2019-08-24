// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_GRAPHICS_CAST_EXTERNAL_BEGIN_FRAME_CLIENT_H_
#define CHROMECAST_GRAPHICS_CAST_EXTERNAL_BEGIN_FRAME_CLIENT_H_

#include "ui/compositor/external_begin_frame_client.h"

#include "base/time/time.h"
#include "base/timer/timer.h"

namespace chromecast {

class CastWindowManagerAura;

class CastExternalBeginFrameClient : public ui::ExternalBeginFrameClient {
 public:
  CastExternalBeginFrameClient(CastWindowManagerAura* cast_window_manager_aura);
  ~CastExternalBeginFrameClient() override;

  // ui::ExternalBeginFrameClient implementation:
  void OnDisplayDidFinishFrame(const viz::BeginFrameAck& ack) override;
  void OnNeedsExternalBeginFrames(bool needs_begin_frames) override;

 private:
  void IssueExternalBeginFrame();

  CastWindowManagerAura* cast_window_manager_aura_;
  uint64_t sequence_number_;
  const uint64_t source_id_;
  const base::TimeDelta interval_;
  bool frame_in_flight_;
  base::RepeatingTimer timer_;

  DISALLOW_COPY_AND_ASSIGN(CastExternalBeginFrameClient);
};

}  // namespace chromecast

#endif  // CHROMECAST_GRAPHICS_CAST_EXTERNAL_BEGIN_FRAME_CLIENT_H_
