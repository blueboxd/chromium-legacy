// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/widget_base.h"

#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/widget/widget_base_client.h"

namespace blink {

WidgetBase::WidgetBase(
    WidgetBaseClient* client,
    CrossVariantMojoAssociatedRemote<mojom::WidgetHostInterfaceBase>
        widget_host,
    CrossVariantMojoAssociatedReceiver<mojom::WidgetInterfaceBase> widget)
    : client_(client),
      widget_host_(std::move(widget_host)),
      receiver_(this, std::move(widget)) {}

WidgetBase::~WidgetBase() = default;

void WidgetBase::SetCompositorHosts(cc::LayerTreeHost* layer_tree_host,
                                    cc::AnimationHost* animation_host) {
  layer_tree_host_ = layer_tree_host;
  animation_host_ = animation_host;
}

cc::LayerTreeHost* WidgetBase::LayerTreeHost() const {
  return layer_tree_host_;
}

cc::AnimationHost* WidgetBase::AnimationHost() const {
  return animation_host_;
}

void WidgetBase::SetCompositorVisible(bool visible) {
  if (visible)
    was_shown_time_ = base::TimeTicks::Now();
  else
    first_update_visual_state_after_hidden_ = true;
}

void WidgetBase::UpdateVisualState() {
  // When recording main frame metrics set the lifecycle reason to
  // kBeginMainFrame, because this is the calller of UpdateLifecycle
  // for the main frame. Otherwise, set the reason to kTests, which is
  // the only other reason this method is called.
  DocumentUpdateReason lifecycle_reason =
      ShouldRecordBeginMainFrameMetrics()
          ? DocumentUpdateReason::kBeginMainFrame
          : DocumentUpdateReason::kTest;
  client_->UpdateLifecycle(WebLifecycleUpdate::kAll, lifecycle_reason);
  client_->SetSuppressFrameRequestsWorkaroundFor704763Only(false);
  if (first_update_visual_state_after_hidden_) {
    client_->RecordTimeToFirstActivePaint(base::TimeTicks::Now() -
                                          was_shown_time_);
    first_update_visual_state_after_hidden_ = false;
  }
}

void WidgetBase::WillBeginCompositorFrame() {
  client_->SetSuppressFrameRequestsWorkaroundFor704763Only(true);
}

void WidgetBase::BeginMainFrame(base::TimeTicks frame_time) {
  client_->DispatchRafAlignedInput(frame_time);
  client_->BeginMainFrame(frame_time);
}

bool WidgetBase::ShouldRecordBeginMainFrameMetrics() {
  // We record metrics only when running in multi-threaded mode, not
  // single-thread mode for testing.
  return Thread::CompositorThread();
}

}  // namespace blink
