// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_INPUT_SYNCHRONOUS_LAYER_TREE_FRAME_SINK_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_INPUT_SYNCHRONOUS_LAYER_TREE_FRAME_SINK_H_

#include "cc/trees/layer_tree_frame_sink.h"
#include "components/viz/common/frame_timing_details_map.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"

class SkCanvas;

namespace gfx {
class Rect;
class Transform;
}  // namespace gfx

namespace viz {
class CompositorFrame;
struct BeginFrameArgs;
}  // namespace viz

namespace blink {

// This class represents the client interface for the frame sink
// created for the synchronous compositor.
class SynchronousLayerTreeFrameSinkClient {
 public:
  virtual void DidActivatePendingTree() = 0;
  virtual void Invalidate(bool needs_draw) = 0;
  virtual void SubmitCompositorFrame(
      uint32_t layer_tree_frame_sink_id,
      base::Optional<viz::CompositorFrame> frame,
      base::Optional<viz::HitTestRegionList> hit_test_region_list) = 0;
  virtual void SetNeedsBeginFrames(bool needs_begin_frames) = 0;
  virtual void SinkDestroyed() = 0;

 protected:
  virtual ~SynchronousLayerTreeFrameSinkClient() {}
};

// This class represents the interface for the frame sink for the synchronous
// compositor.
class SynchronousLayerTreeFrameSink : public cc::LayerTreeFrameSink {
 public:
  using cc::LayerTreeFrameSink::LayerTreeFrameSink;

  virtual void SetSyncClient(
      SynchronousLayerTreeFrameSinkClient* compositor) = 0;
  virtual void DidPresentCompositorFrame(
      const viz::FrameTimingDetailsMap& timing_details) = 0;
  virtual void BeginFrame(const viz::BeginFrameArgs& args) = 0;
  virtual void SetBeginFrameSourcePaused(bool paused) = 0;
  virtual void SetMemoryPolicy(size_t bytes_limit) = 0;
  virtual void ReclaimResources(
      uint32_t layer_tree_frame_sink_id,
      const std::vector<viz::ReturnedResource>& resources) = 0;
  virtual void DemandDrawHw(
      const gfx::Size& viewport_size,
      const gfx::Rect& viewport_rect_for_tile_priority,
      const gfx::Transform& transform_for_tile_priority) = 0;
  virtual void DemandDrawSw(SkCanvas* canvas) = 0;
  virtual void DemandDrawSwZeroCopy() = 0;
  virtual void WillSkipDraw() = 0;
  virtual bool UseZeroCopySoftwareDraw() = 0;
};

}  // namespace blink

#endif  // CONTENT_RENDERER_INPUT_SYNCHRONOUS_LAYER_TREE_FRAME_SINK_H_
