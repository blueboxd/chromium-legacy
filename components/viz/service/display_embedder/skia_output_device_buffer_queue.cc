// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_buffer_queue.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "components/viz/common/switches.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/config/gpu_finch_features.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "ui/gl/gl_surface.h"

namespace viz {

SkiaOutputDeviceBufferQueue::SkiaOutputDeviceBufferQueue(
    std::unique_ptr<OutputPresenter> presenter,
    SkiaOutputSurfaceDependency* deps,
    gpu::MemoryTracker* memory_tracker,
    const DidSwapBufferCompleteCallback& did_swap_buffer_complete_callback)
    : SkiaOutputDevice(deps->GetSharedContextState()->gr_context(),
                       memory_tracker,
                       did_swap_buffer_complete_callback),
      presenter_(std::move(presenter)),
      dependency_(deps) {
  capabilities_.uses_default_gl_framebuffer = false;
  capabilities_.preserve_buffer_content = true;
  capabilities_.only_invalidates_damage_rect = false;
  capabilities_.number_of_buffers = 3;
  capabilities_.orientation_mode = OutputSurface::OrientationMode::kHardware;
#if defined(OS_ANDROID)
  // With vulkan, if the chrome is launched in landscape mode, the chrome is
  // always blank until chrome window is rotated once. Workaround this problem
  // by using logic rotation mode.
  // TODO(https://crbug.com/1115065): use hardware orientation mode for vulkan,
  if (dependency_->GetSharedContextState()->GrContextIsVulkan() &&
      base::FeatureList::GetFieldTrial(features::kVulkan)) {
    capabilities_.orientation_mode = OutputSurface::OrientationMode::kLogic;
  }
#endif

  // Force the number of max pending frames to one when the switch
  // "double-buffer-compositing" is passed.
  // This will keep compositing in double buffered mode assuming |buffer_queue|
  // allocates at most one additional buffer.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDoubleBufferCompositing))
    capabilities_.number_of_buffers = 2;
  capabilities_.max_frames_pending = capabilities_.number_of_buffers - 1;

  presenter_->InitializeCapabilities(&capabilities_);
}

SkiaOutputDeviceBufferQueue::~SkiaOutputDeviceBufferQueue() {
  FreeAllSurfaces();
  // Clear and cancel swap_completion_callbacks_ to free all resource bind to
  // callbacks.
  swap_completion_callbacks_.clear();
}

OutputPresenter::Image* SkiaOutputDeviceBufferQueue::GetNextImage() {
  DCHECK(!available_images_.empty());
  auto* image = available_images_.front();
  available_images_.pop_front();
  return image;
}

void SkiaOutputDeviceBufferQueue::PageFlipComplete(
    OutputPresenter::Image* image) {
  if (displayed_image_) {
    DCHECK_EQ(displayed_image_->skia_representation()->size(), image_size_);
    DCHECK_EQ(displayed_image_->present_count() > 1, displayed_image_ == image);
    displayed_image_->EndPresent();
    if (!displayed_image_->present_count()) {
      available_images_.push_back(displayed_image_);
      // Call BeginWriteSkia() for the next frame here to avoid some expensive
      // operations on the critical code path.
      auto shared_context_state = dependency_->GetSharedContextState();
      if (!available_images_.front()->sk_surface() &&
          shared_context_state->MakeCurrent(nullptr)) {
        // BeginWriteSkia() may alter GL's state.
        shared_context_state->set_need_context_state_reset(true);
        available_images_.front()->BeginWriteSkia();
      }
    }
  }

  displayed_image_ = image;
  swap_completion_callbacks_.pop_front();
}

void SkiaOutputDeviceBufferQueue::FreeAllSurfaces() {
  images_.clear();
  current_image_ = nullptr;
  submitted_image_ = nullptr;
  displayed_image_ = nullptr;
  available_images_.clear();
}

bool SkiaOutputDeviceBufferQueue::IsPrimaryPlaneOverlay() const {
  return true;
}

void SkiaOutputDeviceBufferQueue::SchedulePrimaryPlane(
    const base::Optional<OverlayProcessorInterface::OutputSurfaceOverlayPlane>&
        plane) {
  if (plane) {
    // If the current_image_ is nullptr, it means there is no change on the
    // primary plane. So we just need to schedule the last submitted image.
    auto* image = current_image_ ? current_image_ : submitted_image_;
    DCHECK(image);

    image->BeginPresent();
    presenter_->SchedulePrimaryPlane(plane.value(), image,
                                     image == submitted_image_);
  } else {
    current_frame_has_no_primary_plane_ = true;
  }
}

void SkiaOutputDeviceBufferQueue::ScheduleOverlays(
    SkiaOutputSurface::OverlayList overlays) {
  DCHECK(pending_overlays_.empty());
  pending_overlays_ = presenter_->ScheduleOverlays(std::move(overlays));
}

void SkiaOutputDeviceBufferQueue::PreGrContextSubmit() {
  // The current image may be missing, for example during WebXR presentation.
  if (current_image_)
    current_image_->PreGrContextSubmit();
}

void SkiaOutputDeviceBufferQueue::SwapBuffers(
    BufferPresentedCallback feedback,
    std::vector<ui::LatencyInfo> latency_info) {
  StartSwapBuffers({});

  if (current_frame_has_no_primary_plane_) {
    DCHECK(!current_image_);
    submitted_image_ = nullptr;
    current_frame_has_no_primary_plane_ = false;
  } else {
    DCHECK(current_image_);
    submitted_image_ = current_image_;
    current_image_ = nullptr;
  }

  // Cancelable callback uses weak ptr to drop this task upon destruction.
  // Thus it is safe to use |base::Unretained(this)|.
  // Bind submitted_image_->GetWeakPtr(), since the |submitted_image_| could
  // be released due to reshape() or destruction.
  swap_completion_callbacks_.emplace_back(
      std::make_unique<CancelableSwapCompletionCallback>(base::BindOnce(
          &SkiaOutputDeviceBufferQueue::DoFinishSwapBuffers,
          base::Unretained(this), image_size_, std::move(latency_info),
          submitted_image_ ? submitted_image_->GetWeakPtr() : nullptr,
          std::move(committed_overlays_))));
  presenter_->SwapBuffers(swap_completion_callbacks_.back()->callback(),
                          std::move(feedback));
  committed_overlays_.clear();
  std::swap(committed_overlays_, pending_overlays_);
}

void SkiaOutputDeviceBufferQueue::PostSubBuffer(
    const gfx::Rect& rect,
    BufferPresentedCallback feedback,
    std::vector<ui::LatencyInfo> latency_info) {
  StartSwapBuffers({});

  if (current_frame_has_no_primary_plane_) {
    DCHECK(!current_image_);
    submitted_image_ = nullptr;
    current_frame_has_no_primary_plane_ = false;
  } else {
    if (current_image_) {
      submitted_image_ = current_image_;
      current_image_ = nullptr;
    }
    DCHECK(submitted_image_);
  }

  // Cancelable callback uses weak ptr to drop this task upon destruction.
  // Thus it is safe to use |base::Unretained(this)|.
  // Bind submitted_image_->GetWeakPtr(), since the |submitted_image_| could
  // be released due to reshape() or destruction.
  swap_completion_callbacks_.emplace_back(
      std::make_unique<CancelableSwapCompletionCallback>(base::BindOnce(
          &SkiaOutputDeviceBufferQueue::DoFinishSwapBuffers,
          base::Unretained(this), image_size_, std::move(latency_info),
          submitted_image_ ? submitted_image_->GetWeakPtr() : nullptr,
          std::move(committed_overlays_))));
  presenter_->PostSubBuffer(rect, swap_completion_callbacks_.back()->callback(),
                            std::move(feedback));

  committed_overlays_.clear();
  std::swap(committed_overlays_, pending_overlays_);
}

void SkiaOutputDeviceBufferQueue::CommitOverlayPlanes(
    BufferPresentedCallback feedback,
    std::vector<ui::LatencyInfo> latency_info) {
  StartSwapBuffers({});

  // There is no drawing for this frame on the main buffer.
  DCHECK(!current_image_);
  if (current_frame_has_no_primary_plane_) {
    submitted_image_ = nullptr;
    current_frame_has_no_primary_plane_ = false;
  } else {
    DCHECK(submitted_image_);
  }

  // Cancelable callback uses weak ptr to drop this task upon destruction.
  // Thus it is safe to use |base::Unretained(this)|.
  // Bind submitted_image_->GetWeakPtr(), since the |submitted_image_| could
  // be released due to reshape() or destruction.
  swap_completion_callbacks_.emplace_back(
      std::make_unique<CancelableSwapCompletionCallback>(base::BindOnce(
          &SkiaOutputDeviceBufferQueue::DoFinishSwapBuffers,
          base::Unretained(this), image_size_, std::move(latency_info),
          submitted_image_ ? submitted_image_->GetWeakPtr() : nullptr,
          std::move(committed_overlays_))));
  presenter_->CommitOverlayPlanes(swap_completion_callbacks_.back()->callback(),
                                  std::move(feedback));

  committed_overlays_.clear();
  std::swap(committed_overlays_, pending_overlays_);
}

void SkiaOutputDeviceBufferQueue::DoFinishSwapBuffers(
    const gfx::Size& size,
    std::vector<ui::LatencyInfo> latency_info,
    const base::WeakPtr<OutputPresenter::Image>& image,
    std::vector<OutputPresenter::OverlayData> overlays,
    gfx::SwapCompletionResult result) {
  // Remove the no-longer-in-use overlays from
  // |in_use_by_window_server_overlays_|.
  base::EraseIf(in_use_by_window_server_overlays_,
                [](auto& overlay) { return !overlay.IsInUseByWindowServer(); });

  // Move the still-in-use entries from |overlays| to
  // |in_use_by_window_server_overlays_|.
  for (auto& overlay : overlays) {
    if (overlay.IsInUseByWindowServer())
      in_use_by_window_server_overlays_.emplace(std::move(overlay));
  }

  DCHECK(!result.gpu_fence);
  FinishSwapBuffers(std::move(result), size, latency_info);
  PageFlipComplete(image.get());
}

bool SkiaOutputDeviceBufferQueue::Reshape(const gfx::Size& size,
                                          float device_scale_factor,
                                          const gfx::ColorSpace& color_space,
                                          gfx::BufferFormat format,
                                          gfx::OverlayTransform transform) {
  if (!presenter_->Reshape(size, device_scale_factor, color_space, format,
                           transform)) {
    DLOG(ERROR) << "Failed to resize.";
    return false;
  }

  color_space_ = color_space;
  image_size_ = size;
  FreeAllSurfaces();

  images_ = presenter_->AllocateImages(color_space_, image_size_,
                                       capabilities_.number_of_buffers);
  if (images_.empty())
    return false;

  for (auto& image : images_) {
    available_images_.push_back(image.get());
  }

  return true;
}

SkSurface* SkiaOutputDeviceBufferQueue::BeginPaint(
    std::vector<GrBackendSemaphore>* end_semaphores) {
  if (!current_image_)
    current_image_ = GetNextImage();
  if (!current_image_->sk_surface())
    current_image_->BeginWriteSkia();
  *end_semaphores = current_image_->TakeEndWriteSkiaSemaphores();
  return current_image_->sk_surface();
}

void SkiaOutputDeviceBufferQueue::EndPaint() {
  DCHECK(current_image_);
  current_image_->EndWriteSkia();
}

bool SkiaOutputDeviceBufferQueue::OverlayDataComparator::operator()(
    const OutputPresenter::OverlayData& a,
    const OutputPresenter::OverlayData& b) const {
  return a.mailbox() < b.mailbox();
}

}  // namespace viz
