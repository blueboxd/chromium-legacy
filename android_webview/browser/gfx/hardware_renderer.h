// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_HARDWARE_RENDERER_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_HARDWARE_RENDERER_H_

#include <memory>

#include "android_webview/browser/gfx/child_frame.h"
#include "android_webview/browser/gfx/compositor_id.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/viz/common/frame_timing_details_map.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "services/viz/public/interfaces/compositing/compositor_frame_sink.mojom.h"
#include "ui/gfx/color_space.h"

namespace viz {
class CompositorFrameSinkSupport;
class ParentLocalSurfaceIdAllocator;
}

namespace android_webview {

class ChildFrame;
class RenderThreadManager;
class SurfacesInstance;

struct HardwareRendererDrawParams {
  int clip_left;
  int clip_top;
  int clip_right;
  int clip_bottom;
  int width;
  int height;
  float transform[16];
  gfx::ColorSpace color_space;
};

class HardwareRenderer : public viz::mojom::CompositorFrameSinkClient {
 public:
  // Two rules:
  // 1) Never wait on |new_frame| on the UI thread, or in kModeSync. Otherwise
  //    this defeats the purpose of having a future.
  // 2) Never replace a non-empty frames with an empty frame.
  // The only way to do both is to hold up to two frames. This helper function
  // will wait on all frames in the queue, then only keep the last non-empty
  // frame and return the rest (non-empty) frames. It takes care to not drop
  // other data such as readback requests.
  // A common pattern for appending a new frame is:
  // * WaitAndPrune the existing frame, after which there is at most one frame
  //   is left in queue.
  // * Append new frame without waiting on it.
  static ChildFrameQueue WaitAndPruneFrameQueue(ChildFrameQueue* child_frames);

  explicit HardwareRenderer(RenderThreadManager* state);
  ~HardwareRenderer() override;

  void Draw(HardwareRendererDrawParams* params);
  void CommitFrame();

 private:
  // viz::mojom::CompositorFrameSinkClient implementation.
  void DidReceiveCompositorFrameAck(
      const std::vector<viz::ReturnedResource>& resources) override;
  void OnBeginFrame(const viz::BeginFrameArgs& args,
                    const viz::FrameTimingDetailsMap& timing_details) override;
  void ReclaimResources(
      const std::vector<viz::ReturnedResource>& resources) override;
  void OnBeginFramePausedChanged(bool paused) override;

  void ReturnChildFrame(std::unique_ptr<ChildFrame> child_frame);
  void ReturnResourcesToCompositor(
      const std::vector<viz::ReturnedResource>& resources,
      const CompositorID& compositor_id,
      uint32_t layer_tree_frame_sink_id);

  void AllocateSurface();
  void DestroySurface();

  void CreateNewCompositorFrameSinkSupport();

  RenderThreadManager* const render_thread_manager_;

  const scoped_refptr<SurfacesInstance> surfaces_;

  typedef void* EGLContext;
  EGLContext last_egl_context_;

  // Information about last delegated frame.
  gfx::Size surface_size_;
  float device_scale_factor_ = 0;

  // Infromation from UI on last commit.
  gfx::Vector2d scroll_offset_;

  ChildFrameQueue child_frame_queue_;

  // This holds the last ChildFrame received. Contains the frame info of the
  // last frame. The |frame| member is always null since frame has already
  // been submitted.
  std::unique_ptr<ChildFrame> child_frame_;

  viz::FrameSinkId frame_sink_id_;
  const std::unique_ptr<viz::ParentLocalSurfaceIdAllocator>
      parent_local_surface_id_allocator_;
  std::unique_ptr<viz::CompositorFrameSinkSupport> support_;
  viz::LocalSurfaceId child_id_;
  CompositorID compositor_id_;
  // HardwareRenderer guarantees resources are returned in the order of
  // layer_tree_frame_sink_id, and resources for old output surfaces are
  // dropped.
  uint32_t last_committed_layer_tree_frame_sink_id_;
  uint32_t last_submitted_layer_tree_frame_sink_id_;

  DISALLOW_COPY_AND_ASSIGN(HardwareRenderer);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_HARDWARE_RENDERER_H_
