// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/external_begin_frame_source_mojo.h"

namespace viz {

ExternalBeginFrameSourceMojo::ExternalBeginFrameSourceMojo(
    mojom::ExternalBeginFrameControllerAssociatedRequest controller_request,
    uint32_t restart_id)
    : ExternalBeginFrameSource(this, restart_id),
      binding_(this, std::move(controller_request)) {}

ExternalBeginFrameSourceMojo::~ExternalBeginFrameSourceMojo() {
  DCHECK(!display_);
}

void ExternalBeginFrameSourceMojo::IssueExternalBeginFrame(
    const BeginFrameArgs& args,
    bool force,
    base::OnceCallback<void(const BeginFrameAck&)> callback) {
  OnBeginFrame(args);

  DCHECK(!pending_frame_callback_) << "Got overlapping IssueExternalBeginFrame";
  pending_frame_callback_ = std::move(callback);

  // Ensure that Display will receive the BeginFrame (as a missed one), even
  // if it doesn't currently need it. This way, we ensure that
  // OnDisplayDidFinishFrame will be called for this BeginFrame.
  if (force)
    display_->SetNeedsOneBeginFrame();
}

void ExternalBeginFrameSourceMojo::OnDisplayDidFinishFrame(
    const BeginFrameAck& ack) {
  if (!pending_frame_callback_)
    return;
  std::move(pending_frame_callback_).Run(ack);
}

void ExternalBeginFrameSourceMojo::OnDisplayDestroyed() {
  // As part of destruction, we are automatically removed as a display
  // observer. No need to call RemoveObserver.
  display_ = nullptr;
}

void ExternalBeginFrameSourceMojo::SetDisplay(Display* display) {
  if (display_)
    display_->RemoveObserver(this);
  display_ = display;
  if (display_)
    display_->AddObserver(this);
}

}  // namespace viz
