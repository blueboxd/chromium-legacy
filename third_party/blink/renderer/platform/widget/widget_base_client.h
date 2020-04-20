// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_WIDGET_BASE_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_WIDGET_BASE_CLIENT_H_

#include "base/time/time.h"
#include "cc/paint/element_id.h"
#include "third_party/blink/public/common/metrics/document_update_reason.h"
#include "third_party/blink/public/web/web_lifecycle_update.h"

namespace cc {
class LayerTreeFrameSink;
struct BeginMainFrameMetrics;
class RenderFrameMetadataObserver;
}  // namespace cc

namespace blink {

// This class is part of the foundation of all widgets. It provides
// callbacks from the compositing infrastructure that the individual widgets
// will need to implement.
class WidgetBaseClient {
 public:
  // Dispatch any pending input. This method will called before
  // dispatching a RequestAnimationFrame to the widget.
  virtual void DispatchRafAlignedInput(base::TimeTicks frame_time) = 0;

  // Called to update the document lifecycle, advance the state of animations
  // and dispatch rAF.
  virtual void BeginMainFrame(base::TimeTicks frame_time) = 0;

  // Called to record the time between when the widget was marked visible
  // until the compositor begain producing a frame.
  virtual void RecordTimeToFirstActivePaint(base::TimeDelta duration) = 0;

  // Requests that the lifecycle of the widget be updated.
  virtual void UpdateLifecycle(WebLifecycleUpdate requested_update,
                               DocumentUpdateReason reason) = 0;

  // TODO(crbug.com/704763): Remove the need for this.
  virtual void SetSuppressFrameRequestsWorkaroundFor704763Only(bool) {}

  // Called when main frame metrics are desired. The local frame's UKM
  // aggregator must be informed that collection is starting for the
  // frame.
  virtual void RecordStartOfFrameMetrics() {}

  // Called when a main frame time metric should be emitted, along with
  // any metrics that depend upon the main frame total time.
  virtual void RecordEndOfFrameMetrics(
      base::TimeTicks frame_begin_time,
      cc::ActiveFrameSequenceTrackers trackers) {}

  // Return metrics information for the stages of BeginMainFrame. This is
  // ultimately implemented by Blink's LocalFrameUKMAggregator. It must be a
  // distinct call from the FrameMetrics above because the BeginMainFrameMetrics
  // for compositor latency must be gathered before the layer tree is
  // committed to the compositor, which is before the call to
  // RecordEndOfFrameMetrics.
  virtual std::unique_ptr<cc::BeginMainFrameMetrics>
  GetBeginMainFrameMetrics() {
    return nullptr;
  }

  // Methods called to mark the beginning and end of the
  // LayerTreeHost::UpdateLayers method. Only called when gathering main frame
  // UMA and UKM. That is, when RecordStartOfFrameMetrics has been called, and
  // before RecordEndOfFrameMetrics has been called.
  virtual void BeginUpdateLayers() {}
  virtual void EndUpdateLayers() {}

  // Methods called to mark the beginning and end of a commit to the impl
  // thread for a frame. Only called when gathering main frame
  // UMA and UKM. That is, when RecordStartOfFrameMetrics has been called, and
  // before RecordEndOfFrameMetrics has been called.
  virtual void BeginCommitCompositorFrame() {}
  virtual void EndCommitCompositorFrame(base::TimeTicks commit_start_time) {}

  // Applies viewport related properties during a commit from the compositor
  // thread.
  virtual void ApplyViewportChanges(const cc::ApplyViewportChangesArgs& args) {}

  virtual void RecordManipulationTypeCounts(cc::ManipulationInfo info) {}

  virtual void SendOverscrollEventFromImplSide(
      const gfx::Vector2dF& overscroll_delta,
      cc::ElementId scroll_latched_element_id) {}
  virtual void SendScrollEndEventFromImplSide(
      cc::ElementId scroll_latched_element_id) {}

  virtual void DidBeginMainFrame() {}
  virtual void DidCommitAndDrawCompositorFrame() {}

  virtual void OnDeferMainFrameUpdatesChanged(bool defer) {}
  virtual void OnDeferCommitsChanged(bool defer) {}

  using LayerTreeFrameSinkCallback = base::OnceCallback<void(
      std::unique_ptr<cc::LayerTreeFrameSink>,
      std::unique_ptr<cc::RenderFrameMetadataObserver>)>;

  // Requests a LayerTreeFrameSink to submit CompositorFrames to.
  virtual void RequestNewLayerTreeFrameSink(
      LayerTreeFrameSinkCallback callback) = 0;

  virtual void WillBeginMainFrame() {}
  virtual void DidCompletePageScaleAnimation() {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_WIDGET_BASE_CLIENT_H_
