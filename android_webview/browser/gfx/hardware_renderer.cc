// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/hardware_renderer.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>

#include "android_webview/browser/gfx/aw_gl_surface.h"
#include "android_webview/browser/gfx/aw_render_thread_context_provider.h"
#include "android_webview/browser/gfx/parent_compositor_draw_constraints.h"
#include "android_webview/browser/gfx/render_thread_manager.h"
#include "android_webview/browser/gfx/surfaces_instance.h"
#include "android_webview/common/aw_switches.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "ui/gfx/transform.h"
#include "ui/gl/gl_bindings.h"

namespace android_webview {

HardwareRenderer::HardwareRenderer(RenderThreadManager* state)
    : render_thread_manager_(state),
      surfaces_(SurfacesInstance::GetOrCreateInstance()),
      last_egl_context_(surfaces_->is_using_vulkan() ? nullptr
                                                     : eglGetCurrentContext()),
      frame_sink_id_(surfaces_->AllocateFrameSinkId()),
      parent_local_surface_id_allocator_(
          std::make_unique<viz::ParentLocalSurfaceIdAllocator>()),
      last_committed_layer_tree_frame_sink_id_(0u),
      last_submitted_layer_tree_frame_sink_id_(0u) {
  DCHECK(surfaces_->is_using_vulkan() || last_egl_context_);
  surfaces_->GetFrameSinkManager()->RegisterFrameSinkId(
      frame_sink_id_, true /* report_activation */);
  surfaces_->GetFrameSinkManager()->SetFrameSinkDebugLabel(frame_sink_id_,
                                                           "HardwareRenderer");
  CreateNewCompositorFrameSinkSupport();
}

HardwareRenderer::~HardwareRenderer() {
  // Must reset everything before |surface_factory_| to ensure all
  // resources are returned before resetting.
  if (child_id_.is_valid())
    DestroySurface();
  support_.reset();
  surfaces_->GetFrameSinkManager()->InvalidateFrameSinkId(frame_sink_id_);

  // Reset draw constraints.
  render_thread_manager_->PostParentDrawDataToChildCompositorOnRT(
      ParentCompositorDrawConstraints(), compositor_id_,
      viz::FrameTimingDetailsMap(), 0u);
  for (auto& child_frame : child_frame_queue_) {
    child_frame->WaitOnFutureIfNeeded();
    ReturnChildFrame(std::move(child_frame));
  }
}

void HardwareRenderer::CommitFrame() {
  TRACE_EVENT0("android_webview", "CommitFrame");
  scroll_offset_ = render_thread_manager_->GetScrollOffsetOnRT();
  ChildFrameQueue child_frames = render_thread_manager_->PassFramesOnRT();
  // |child_frames| should have at most one non-empty frame, and one current
  // and unwaited frame, in that order.
  DCHECK_LE(child_frames.size(), 2u);
  if (child_frames.empty())
    return;
  // Insert all except last, ie current frame.
  while (child_frames.size() > 1u) {
    child_frame_queue_.emplace_back(std::move(child_frames.front()));
    child_frames.pop_front();
  }
  for (auto& pruned_frame : WaitAndPruneFrameQueue(&child_frame_queue_))
    ReturnChildFrame(std::move(pruned_frame));
  DCHECK_LE(child_frame_queue_.size(), 1u);
  child_frame_queue_.emplace_back(std::move(child_frames.front()));
}

void HardwareRenderer::Draw(HardwareRendererDrawParams* params) {
  TRACE_EVENT1("android_webview", "HardwareRenderer::Draw", "vulkan",
               surfaces_->is_using_vulkan());

  for (auto& pruned_frame : WaitAndPruneFrameQueue(&child_frame_queue_))
    ReturnChildFrame(std::move(pruned_frame));
  DCHECK_LE(child_frame_queue_.size(), 1u);
  if (!child_frame_queue_.empty()) {
    child_frame_ = std::move(child_frame_queue_.front());
    child_frame_queue_.clear();
  }
  if (child_frame_) {
    last_committed_layer_tree_frame_sink_id_ =
        child_frame_->layer_tree_frame_sink_id;
  }

  if (!surfaces_->is_using_vulkan()) {
    // We need to watch if the current Android context has changed and enforce a
    // clean-up in the compositor.
    EGLContext current_context = eglGetCurrentContext();
    DCHECK(current_context) << "Draw called without EGLContext";

    // TODO(boliu): Handle context loss.
    if (last_egl_context_ != current_context)
      DLOG(WARNING) << "EGLContextChanged";
  }

  bool submitted_new_frame = false;
  uint32_t frame_token = 0u;
  // SurfaceFactory::SubmitCompositorFrame might call glFlush. So calling it
  // during "kModeSync" stage (which does not allow GL) might result in extra
  // kModeProcess. Instead, submit the frame in "kModeDraw" stage to avoid
  // unnecessary kModeProcess.
  if (child_frame_.get() && child_frame_->frame.get()) {
    if (!compositor_id_.Equals(child_frame_->compositor_id) ||
        last_submitted_layer_tree_frame_sink_id_ !=
            child_frame_->layer_tree_frame_sink_id) {
      if (child_id_.is_valid())
        DestroySurface();

      CreateNewCompositorFrameSinkSupport();
      compositor_id_ = child_frame_->compositor_id;
      last_submitted_layer_tree_frame_sink_id_ =
          child_frame_->layer_tree_frame_sink_id;
    }

    std::unique_ptr<viz::CompositorFrame> child_compositor_frame =
        std::move(child_frame_->frame);

    float device_scale_factor = child_compositor_frame->device_scale_factor();
    gfx::Size frame_size = child_compositor_frame->size_in_pixels();
    if (!child_id_.is_valid() || surface_size_ != frame_size ||
        device_scale_factor_ != device_scale_factor) {
      if (child_id_.is_valid())
        DestroySurface();
      AllocateSurface();
      surface_size_ = frame_size;
      device_scale_factor_ = device_scale_factor;
    }

    frame_token = child_compositor_frame->metadata.frame_token;
    support_->SubmitCompositorFrame(child_id_,
                                    std::move(*child_compositor_frame));
    submitted_new_frame = true;
  }

  gfx::Transform transform(gfx::Transform::kSkipInitialization);
  transform.matrix().setColMajorf(params->transform);
  transform.Translate(scroll_offset_.x(), scroll_offset_.y());

  gfx::Size viewport(params->width, params->height);
  // Need to post the new transform matrix back to child compositor
  // because there is no onDraw during a Render Thread animation, and child
  // compositor might not have the tiles rasterized as the animation goes on.
  ParentCompositorDrawConstraints draw_constraints(viewport, transform);
  bool need_to_update_draw_constraints =
      !child_frame_.get() || draw_constraints.NeedUpdate(*child_frame_);

  // Post data after draw if submitted_new_frame, since we may have
  // presentation feedback to return as well.
  if (need_to_update_draw_constraints && !submitted_new_frame) {
    render_thread_manager_->PostParentDrawDataToChildCompositorOnRT(
        draw_constraints, compositor_id_, viz::FrameTimingDetailsMap(), 0u);
  }

  if (!child_id_.is_valid())
    return;

  CopyOutputRequestQueue requests;
  requests.swap(child_frame_->copy_requests);
  for (auto& copy_request : requests) {
    support_->RequestCopyOfOutput(child_id_, std::move(copy_request));
  }

  gfx::Rect clip(params->clip_left, params->clip_top,
                 params->clip_right - params->clip_left,
                 params->clip_bottom - params->clip_top);
  surfaces_->DrawAndSwap(viewport, clip, transform, surface_size_,
                         viz::SurfaceId(frame_sink_id_, child_id_),
                         device_scale_factor_, params->color_space);
  viz::FrameTimingDetailsMap timing_details =
      support_->TakeFrameTimingDetailsMap();
  if (submitted_new_frame) {
    render_thread_manager_->PostParentDrawDataToChildCompositorOnRT(
        draw_constraints, compositor_id_, std::move(timing_details),
        frame_token);
  }
}

void HardwareRenderer::AllocateSurface() {
  DCHECK(!child_id_.is_valid());
  parent_local_surface_id_allocator_->GenerateId();
  child_id_ =
      parent_local_surface_id_allocator_->GetCurrentLocalSurfaceIdAllocation()
          .local_surface_id();
  surfaces_->AddChildId(viz::SurfaceId(frame_sink_id_, child_id_));
}

void HardwareRenderer::DestroySurface() {
  DCHECK(child_id_.is_valid());

  surfaces_->RemoveChildId(viz::SurfaceId(frame_sink_id_, child_id_));
  support_->EvictSurface(child_id_);
  child_id_ = viz::LocalSurfaceId();
  surfaces_->GetFrameSinkManager()->surface_manager()->GarbageCollectSurfaces();
}

void HardwareRenderer::DidReceiveCompositorFrameAck(
    const std::vector<viz::ReturnedResource>& resources) {
  ReturnResourcesToCompositor(resources, compositor_id_,
                              last_submitted_layer_tree_frame_sink_id_);
}

void HardwareRenderer::OnBeginFrame(
    const viz::BeginFrameArgs& args,
    const viz::FrameTimingDetailsMap& timing_details) {
  NOTREACHED();
}

void HardwareRenderer::ReclaimResources(
    const std::vector<viz::ReturnedResource>& resources) {
  ReturnResourcesToCompositor(resources, compositor_id_,
                              last_submitted_layer_tree_frame_sink_id_);
}

void HardwareRenderer::OnBeginFramePausedChanged(bool paused) {}

namespace {

void MoveCopyRequests(CopyOutputRequestQueue* from,
                      CopyOutputRequestQueue* to) {
  std::move(from->begin(), from->end(), std::back_inserter(*to));
  from->clear();
}

}  // namespace

// static
ChildFrameQueue HardwareRenderer::WaitAndPruneFrameQueue(
    ChildFrameQueue* child_frames_ptr) {
  ChildFrameQueue& child_frames = *child_frames_ptr;
  ChildFrameQueue pruned_frames;
  if (child_frames.empty())
    return pruned_frames;

  // First find the last non-empty frame.
  int remaining_frame_index = -1;
  for (size_t i = 0; i < child_frames.size(); ++i) {
    auto& child_frame = *child_frames[i];
    child_frame.WaitOnFutureIfNeeded();
    if (child_frame.frame)
      remaining_frame_index = i;
  }
  // If all empty, keep the last one.
  if (remaining_frame_index < 0)
    remaining_frame_index = child_frames.size() - 1;

  // Prune end.
  while (child_frames.size() > static_cast<size_t>(remaining_frame_index + 1)) {
    std::unique_ptr<ChildFrame> frame = std::move(child_frames.back());
    child_frames.pop_back();
    MoveCopyRequests(&frame->copy_requests,
                     &child_frames[remaining_frame_index]->copy_requests);
    DCHECK(!frame->frame);
  }
  DCHECK_EQ(static_cast<size_t>(remaining_frame_index),
            child_frames.size() - 1);

  // Prune front.
  while (child_frames.size() > 1) {
    std::unique_ptr<ChildFrame> frame = std::move(child_frames.front());
    child_frames.pop_front();
    MoveCopyRequests(&frame->copy_requests,
                     &child_frames.back()->copy_requests);
    if (frame->frame)
      pruned_frames.emplace_back(std::move(frame));
  }
  return pruned_frames;
}

void HardwareRenderer::ReturnChildFrame(
    std::unique_ptr<ChildFrame> child_frame) {
  if (!child_frame || !child_frame->frame)
    return;

  std::vector<viz::ReturnedResource> resources_to_return =
      viz::TransferableResource::ReturnResources(
          child_frame->frame->resource_list);

  // The child frame's compositor id is not necessarily same as
  // compositor_id_.
  ReturnResourcesToCompositor(resources_to_return, child_frame->compositor_id,
                              child_frame->layer_tree_frame_sink_id);
}

void HardwareRenderer::ReturnResourcesToCompositor(
    const std::vector<viz::ReturnedResource>& resources,
    const CompositorID& compositor_id,
    uint32_t layer_tree_frame_sink_id) {
  if (layer_tree_frame_sink_id != last_committed_layer_tree_frame_sink_id_)
    return;
  render_thread_manager_->InsertReturnedResourcesOnRT(resources, compositor_id,
                                                      layer_tree_frame_sink_id);
}

void HardwareRenderer::CreateNewCompositorFrameSinkSupport() {
  constexpr bool is_root = false;
  constexpr bool needs_sync_points = true;
  support_.reset();
  support_ = std::make_unique<viz::CompositorFrameSinkSupport>(
      this, surfaces_->GetFrameSinkManager(), frame_sink_id_, is_root,
      needs_sync_points);
}

}  // namespace android_webview
