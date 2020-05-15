// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_buffer_queue.h"

#include "base/command_line.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/switches.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/ipc/common/gpu_surface_lookup.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gl_surface.h"

#if defined(OS_ANDROID)
#include "ui/gl/gl_surface_egl_surface_control.h"
#endif

namespace viz {
namespace {

constexpr uint32_t kSharedImageUsage =
    gpu::SHARED_IMAGE_USAGE_SCANOUT | gpu::SHARED_IMAGE_USAGE_DISPLAY |
    gpu::SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT;

}  // namespace

class SkiaOutputDeviceBufferQueue::Image {
 public:
  Image(gpu::SharedImageFactory* factory,
        gpu::SharedImageRepresentationFactory* representation_factory)
      : factory_(factory), representation_factory_(representation_factory) {}
  ~Image() {
    // TODO(vasilyt): As we are going to delete image anyway we should be able
    // to abort write to avoid unnecessary flush to submit semaphores.
    if (scoped_skia_write_access_) {
      EndWriteSkia();
    }
    DCHECK(!scoped_skia_write_access_);
  }

  bool Initialize(const gfx::Size& size,
                  const gfx::ColorSpace& color_space,
                  ResourceFormat format,
                  SkiaOutputSurfaceDependency* deps,
                  uint32_t shared_image_usage) {
    auto mailbox = gpu::Mailbox::GenerateForSharedImage();
    // TODO(penghuang): This should pass the surface handle for ChromeOS
    if (!factory_->CreateSharedImage(mailbox, format, size, color_space,
                                     gpu::kNullSurfaceHandle,
                                     shared_image_usage)) {
      DLOG(ERROR) << "CreateSharedImage failed.";
      return false;
    }

    // Initialize |shared_image_deletor_| to make sure the shared image backing
    // will be released with the Image.
    shared_image_deletor_.ReplaceClosure(base::BindOnce(
        base::IgnoreResult(&gpu::SharedImageFactory::DestroySharedImage),
        base::Unretained(factory_), mailbox));

    skia_representation_ = representation_factory_->ProduceSkia(
        mailbox, deps->GetSharedContextState());
    if (!skia_representation_) {
      DLOG(ERROR) << "ProduceSkia() failed.";
      return false;
    }

    overlay_representation_ = representation_factory_->ProduceOverlay(mailbox);

    // If the backing doesn't support overlay, then fallback to GL.
    if (!overlay_representation_)
      gl_representation_ = representation_factory_->ProduceGLTexture(mailbox);

    if (!overlay_representation_ && !gl_representation_) {
      DLOG(ERROR) << "ProduceOverlay() and ProduceGLTexture() failed.";
      return false;
    }

    return true;
  }

  void BeginWriteSkia() {
    DCHECK(!scoped_skia_write_access_);
    DCHECK(!scoped_overlay_read_access_);
    DCHECK(end_semaphores_.empty());

    std::vector<GrBackendSemaphore> begin_semaphores;
    // LegacyFontHost will get LCD text and skia figures out what type to use.
    SkSurfaceProps surface_props(0 /* flags */,
                                 SkSurfaceProps::kLegacyFontHost_InitType);

    // Buffer queue is internal to GPU proc and handles texture initialization,
    // so allow uncleared access.
    // TODO(vasilyt): Props and MSAA
    scoped_skia_write_access_ = skia_representation_->BeginScopedWriteAccess(
        0 /* final_msaa_count */, surface_props, &begin_semaphores,
        &end_semaphores_,
        gpu::SharedImageRepresentation::AllowUnclearedAccess::kYes);
    DCHECK(scoped_skia_write_access_);
    if (!begin_semaphores.empty()) {
      scoped_skia_write_access_->surface()->wait(begin_semaphores.size(),
                                                 begin_semaphores.data());
    }
  }

  SkSurface* sk_surface() {
    return scoped_skia_write_access_ ? scoped_skia_write_access_->surface()
                                     : nullptr;
  }

  std::vector<GrBackendSemaphore> TakeEndWriteSkiaSemaphores() {
    std::vector<GrBackendSemaphore> temp_vector;
    temp_vector.swap(end_semaphores_);
    return temp_vector;
  }

  void EndWriteSkia() {
    // The Flush now takes place in finishPaintCurrentBuffer on the CPU side.
    // check if end_semaphores is not empty then flash here
    DCHECK(scoped_skia_write_access_);
    if (!end_semaphores_.empty()) {
      GrFlushInfo flush_info = {
          .fFlags = kNone_GrFlushFlags,
          .fNumSemaphores = end_semaphores_.size(),
          .fSignalSemaphores = end_semaphores_.data(),
      };
      scoped_skia_write_access_->surface()->flush(
          SkSurface::BackendSurfaceAccess::kNoAccess, flush_info);
    }
    scoped_skia_write_access_.reset();
    end_semaphores_.clear();

    // SkiaRenderer always draws the full frame.
    skia_representation_->SetCleared();
  }

  void BeginPresent() {
    if (++present_count_ != 1) {
      DCHECK(scoped_overlay_read_access_ || scoped_gl_read_access_);
      return;
    }

    DCHECK(!scoped_skia_write_access_);
    DCHECK(!scoped_overlay_read_access_);

    if (overlay_representation_) {
      scoped_overlay_read_access_ =
          overlay_representation_->BeginScopedReadAccess(
              true /* need_gl_image */);
      DCHECK(scoped_overlay_read_access_);
      return;
    }

    scoped_gl_read_access_ = gl_representation_->BeginScopedAccess(
        GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM,
        gpu::SharedImageRepresentation::AllowUnclearedAccess::kNo);
    DCHECK(scoped_gl_read_access_);
  }

  void EndPresent() {
    DCHECK(present_count_);
    if (--present_count_)
      return;
    scoped_overlay_read_access_.reset();
    scoped_gl_read_access_.reset();
  }

  gl::GLImage* GetGLImage(std::unique_ptr<gfx::GpuFence>* fence) {
    if (scoped_overlay_read_access_)
      return scoped_overlay_read_access_->gl_image();

    DCHECK(scoped_gl_read_access_);

    if (gl::GLFence::IsGpuFenceSupported() && fence) {
      if (auto gl_fence = gl::GLFence::CreateForGpuFence())
        *fence = gl_fence->GetGpuFence();
    }
    auto* texture = gl_representation_->GetTexture();
    return texture->GetLevelImage(texture->target(), 0);
  }

  base::WeakPtr<Image> GetWeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

  int present_count() const { return present_count_; }
  gpu::SharedImageRepresentationSkia* skia_representation() {
    return skia_representation_.get();
  }
  const gfx::Size& size() const { return skia_representation_->size(); }

 private:
  gpu::SharedImageFactory* const factory_;
  gpu::SharedImageRepresentationFactory* const representation_factory_;

  base::ScopedClosureRunner shared_image_deletor_;
  std::unique_ptr<gpu::SharedImageRepresentationSkia> skia_representation_;
  std::unique_ptr<gpu::SharedImageRepresentationOverlay>
      overlay_representation_;
  std::unique_ptr<gpu::SharedImageRepresentationGLTexture> gl_representation_;
  std::unique_ptr<gpu::SharedImageRepresentationSkia::ScopedWriteAccess>
      scoped_skia_write_access_;
  std::unique_ptr<gpu::SharedImageRepresentationOverlay::ScopedReadAccess>
      scoped_overlay_read_access_;
  std::unique_ptr<gpu::SharedImageRepresentationGLTexture::ScopedAccess>
      scoped_gl_read_access_;
  std::vector<GrBackendSemaphore> end_semaphores_;
  int present_count_ = 0;
  base::WeakPtrFactory<Image> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(Image);
};

class SkiaOutputDeviceBufferQueue::OverlayData {
 public:
  OverlayData(
      std::unique_ptr<gpu::SharedImageRepresentationOverlay> representation,
      std::unique_ptr<gpu::SharedImageRepresentationOverlay::ScopedReadAccess>
          scoped_read_access)
      : representation_(std::move(representation)),
        scoped_read_access_(std::move(scoped_read_access)) {}
  OverlayData(OverlayData&&) = default;
  ~OverlayData() = default;
  OverlayData& operator=(OverlayData&&) = default;

  gl::GLImage* gl_image() { return scoped_read_access_->gl_image(); }

 private:
  std::unique_ptr<gpu::SharedImageRepresentationOverlay> representation_;
  std::unique_ptr<gpu::SharedImageRepresentationOverlay::ScopedReadAccess>
      scoped_read_access_;
};

SkiaOutputDeviceBufferQueue::SkiaOutputDeviceBufferQueue(
    scoped_refptr<gl::GLSurface> gl_surface,
    SkiaOutputSurfaceDependency* deps,
    gpu::MemoryTracker* memory_tracker,
    const DidSwapBufferCompleteCallback& did_swap_buffer_complete_callback,
    uint32_t shared_image_usage)
    : SkiaOutputDevice(memory_tracker, did_swap_buffer_complete_callback),
      dependency_(deps),
      gl_surface_(std::move(gl_surface)),
      supports_async_swap_(gl_surface_->SupportsAsyncSwap()),
      shared_image_factory_(deps->GetGpuPreferences(),
                            deps->GetGpuDriverBugWorkarounds(),
                            deps->GetGpuFeatureInfo(),
                            deps->GetSharedContextState().get(),
                            deps->GetMailboxManager(),
                            deps->GetSharedImageManager(),
                            deps->GetGpuImageFactory(),
                            memory_tracker,
                            true),
      shared_image_usage_(shared_image_usage) {
  shared_image_representation_factory_ =
      std::make_unique<gpu::SharedImageRepresentationFactory>(
          dependency_->GetSharedImageManager(), memory_tracker);

#if defined(USE_OZONE)
  image_format_ = GetResourceFormat(display::DisplaySnapshot::PrimaryFormat());
#else
  image_format_ = RGBA_8888;
#endif
  // GL is origin is at bottom left normally, all Surfaceless implementations
  // are flipped.
  DCHECK_EQ(gl_surface_->GetOrigin(), gfx::SurfaceOrigin::kTopLeft);

  capabilities_.uses_default_gl_framebuffer = false;
  capabilities_.android_surface_control_feature_enabled = true;
  capabilities_.supports_post_sub_buffer = gl_surface_->SupportsPostSubBuffer();
  capabilities_.supports_commit_overlay_planes =
      gl_surface_->SupportsCommitOverlayPlanes();
  capabilities_.max_frames_pending = 2;

  // Force the number of max pending frames to one when the switch
  // "double-buffer-compositing" is passed.
  // This will keep compositing in double buffered mode assuming |buffer_queue|
  // allocates at most one additional buffer.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDoubleBufferCompositing))
    capabilities_.max_frames_pending = 1;

  capabilities_.only_invalidates_damage_rect = false;
  // Set supports_surfaceless to enable overlays.
  capabilities_.supports_surfaceless = true;
  capabilities_.preserve_buffer_content = true;
  // We expect origin of buffers is at top left.
  capabilities_.output_surface_origin = gfx::SurfaceOrigin::kTopLeft;

  // TODO(penghuang): Use defaultBackendFormat() in shared image implementation
  // to make sure backend formant is consistent.
  capabilities_.sk_color_type = ResourceFormatToClosestSkColorType(
      true /* gpu_compositing */, image_format_);
  capabilities_.gr_backend_format =
      dependency_->GetSharedContextState()->gr_context()->defaultBackendFormat(
          capabilities_.sk_color_type, GrRenderable::kYes);
}

SkiaOutputDeviceBufferQueue::SkiaOutputDeviceBufferQueue(
    scoped_refptr<gl::GLSurface> gl_surface,
    SkiaOutputSurfaceDependency* deps,
    gpu::MemoryTracker* memory_tracker,
    const DidSwapBufferCompleteCallback& did_swap_buffer_complete_callback)
    : SkiaOutputDeviceBufferQueue(std::move(gl_surface),
                                  deps,
                                  memory_tracker,
                                  did_swap_buffer_complete_callback,
                                  kSharedImageUsage) {}

SkiaOutputDeviceBufferQueue::~SkiaOutputDeviceBufferQueue() {
  FreeAllSurfaces();
  // Clear and cancel swap_completion_callbacks_ to free all resource bind to
  // callbacks.
  swap_completion_callbacks_.clear();
}

// static
std::unique_ptr<SkiaOutputDeviceBufferQueue>
SkiaOutputDeviceBufferQueue::Create(
    SkiaOutputSurfaceDependency* deps,
    gpu::MemoryTracker* memory_tracker,
    const DidSwapBufferCompleteCallback& did_swap_buffer_complete_callback) {
#if defined(OS_ANDROID)
  if (deps->GetGpuFeatureInfo()
          .status_values[gpu::GPU_FEATURE_TYPE_ANDROID_SURFACE_CONTROL] !=
      gpu::kGpuFeatureStatusEnabled) {
    return nullptr;
  }

  bool can_be_used_with_surface_control = false;
  ANativeWindow* window =
      gpu::GpuSurfaceLookup::GetInstance()->AcquireNativeWidget(
          deps->GetSurfaceHandle(), &can_be_used_with_surface_control);
  if (!window || !can_be_used_with_surface_control)
    return nullptr;
  // TODO(https://crbug.com/1012401): don't depend on GL.
  auto gl_surface = base::MakeRefCounted<gl::GLSurfaceEGLSurfaceControl>(
      window, base::ThreadTaskRunnerHandle::Get());
  if (!gl_surface->Initialize(gl::GLSurfaceFormat())) {
    LOG(ERROR) << "Failed to initialize GLSurfaceEGLSurfaceControl.";
    return nullptr;
  }

  if (!deps->GetSharedContextState()->MakeCurrent(gl_surface.get(),
                                                  true /* needs_gl*/)) {
    LOG(ERROR) << "MakeCurrent failed.";
    return nullptr;
  }

  return std::make_unique<SkiaOutputDeviceBufferQueue>(
      std::move(gl_surface), deps, memory_tracker,
      did_swap_buffer_complete_callback);
#else
  return nullptr;
#endif
}

SkiaOutputDeviceBufferQueue::Image*
SkiaOutputDeviceBufferQueue::GetNextImage() {
  DCHECK(!available_images_.empty());
  auto* image = available_images_.front();
  available_images_.pop_front();
  return image;
}

void SkiaOutputDeviceBufferQueue::PageFlipComplete(Image* image) {
  if (displayed_image_) {
    DCHECK_EQ(displayed_image_->size(), image_size_);
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
    const OverlayProcessorInterface::OutputSurfaceOverlayPlane& plane) {
  // If the current_image_ is nullptr, it means there is no change on the
  // primary plane. So we just need to schedule the last submitted image.
  auto* image = current_image_ ? current_image_ : submitted_image_;
  DCHECK(image);

  image->BeginPresent();

  std::unique_ptr<gfx::GpuFence> fence;
  // If the submitted_image_ is being scheduled, we don't new a new fence.
  auto* gl_image =
      image->GetGLImage(image == submitted_image_ ? nullptr : &fence);

  // Output surface is also z-order 0.
  constexpr int kPlaneZOrder = 0;
  // Output surface always uses the full texture.
  constexpr gfx::RectF kUVRect(0.f, 0.f, 1.0f, 1.0f);
  gl_surface_->ScheduleOverlayPlane(kPlaneZOrder, plane.transform, gl_image,
                                    ToNearestRect(plane.display_rect), kUVRect,
                                    plane.enable_blending, std::move(fence));
}

void SkiaOutputDeviceBufferQueue::ScheduleOverlays(
    SkiaOutputSurface::OverlayList overlays) {
#if defined(OS_ANDROID)
  DCHECK(pending_overlays_.empty());
  for (auto& overlay : overlays) {
    auto shared_image =
        shared_image_representation_factory_->ProduceOverlay(overlay.mailbox);
    // When display is re-opened, the first few frames might not have video
    // resource ready. Possible investigation crbug.com/1023971.
    if (!shared_image) {
      LOG(ERROR) << "Invalid mailbox.";
      continue;
    }

    std::unique_ptr<gpu::SharedImageRepresentationOverlay::ScopedReadAccess>
        shared_image_access =
            shared_image->BeginScopedReadAccess(true /* needs_gl_image */);
    if (!shared_image_access) {
      LOG(ERROR) << "Could not access SharedImage for read.";
      continue;
    }

    pending_overlays_.emplace_back(std::move(shared_image),
                                   std::move(shared_image_access));
    auto* gl_image = pending_overlays_.back().gl_image();
    DLOG_IF(ERROR, !gl_image) << "Cannot get GLImage.";

    if (gl_image) {
      DCHECK(!overlay.gpu_fence_id);
      gl_surface_->ScheduleOverlayPlane(
          overlay.plane_z_order, overlay.transform, gl_image,
          ToNearestRect(overlay.display_rect), overlay.uv_rect,
          !overlay.is_opaque, nullptr /* gpu_fence */);
    }
  }
#endif  // defined(OS_ANDROID)
}

void SkiaOutputDeviceBufferQueue::SwapBuffers(
    BufferPresentedCallback feedback,
    std::vector<ui::LatencyInfo> latency_info) {
  StartSwapBuffers({});

  DCHECK(current_image_);
  submitted_image_ = current_image_;
  current_image_ = nullptr;

  if (supports_async_swap_) {
    // Cancelable callback uses weak ptr to drop this task upon destruction.
    // Thus it is safe to use |base::Unretained(this)|.
    // Bind submitted_image_->GetWeakPtr(), since the |submitted_image_| could
    // be released due to reshape() or destruction.
    swap_completion_callbacks_.emplace_back(
        std::make_unique<CancelableSwapCompletionCallback>(base::BindOnce(
            &SkiaOutputDeviceBufferQueue::DoFinishSwapBuffers,
            base::Unretained(this), image_size_, std::move(latency_info),
            submitted_image_->GetWeakPtr(), std::move(committed_overlays_))));
    gl_surface_->SwapBuffersAsync(swap_completion_callbacks_.back()->callback(),
                                  std::move(feedback));
  } else {
    DoFinishSwapBuffers(image_size_, std::move(latency_info),
                        submitted_image_->GetWeakPtr(),
                        std::move(committed_overlays_),
                        gl_surface_->SwapBuffers(std::move(feedback)), nullptr);
  }
  committed_overlays_.clear();
  std::swap(committed_overlays_, pending_overlays_);
}

void SkiaOutputDeviceBufferQueue::PostSubBuffer(
    const gfx::Rect& rect,
    BufferPresentedCallback feedback,
    std::vector<ui::LatencyInfo> latency_info) {
  StartSwapBuffers({});

  if (current_image_) {
    submitted_image_ = current_image_;
    current_image_ = nullptr;
  }
  DCHECK(submitted_image_);

  if (supports_async_swap_) {
    // Cancelable callback uses weak ptr to drop this task upon destruction.
    // Thus it is safe to use |base::Unretained(this)|.
    // Bind submitted_image_->GetWeakPtr(), since the |submitted_image_| could
    // be released due to reshape() or destruction.
    swap_completion_callbacks_.emplace_back(
        std::make_unique<CancelableSwapCompletionCallback>(base::BindOnce(
            &SkiaOutputDeviceBufferQueue::DoFinishSwapBuffers,
            base::Unretained(this), image_size_, std::move(latency_info),
            submitted_image_->GetWeakPtr(), std::move(committed_overlays_))));
    gl_surface_->PostSubBufferAsync(
        rect.x(), rect.y(), rect.width(), rect.height(),
        swap_completion_callbacks_.back()->callback(), std::move(feedback));
  } else {
    DoFinishSwapBuffers(
        image_size_, std::move(latency_info), submitted_image_->GetWeakPtr(),
        std::move(committed_overlays_),
        gl_surface_->PostSubBuffer(rect.x(), rect.y(), rect.width(),
                                   rect.height(), std::move(feedback)),
        nullptr);
  }
  committed_overlays_.clear();
  std::swap(committed_overlays_, pending_overlays_);
}

void SkiaOutputDeviceBufferQueue::CommitOverlayPlanes(
    BufferPresentedCallback feedback,
    std::vector<ui::LatencyInfo> latency_info) {
  StartSwapBuffers({});

  // There is no drawing for this frame on the main buffer.
  DCHECK(!current_image_);
  // A main buffer has to be submitted for previous frames.
  DCHECK(submitted_image_);

  if (supports_async_swap_) {
    // Cancelable callback uses weak ptr to drop this task upon destruction.
    // Thus it is safe to use |base::Unretained(this)|.
    // Bind submitted_image_->GetWeakPtr(), since the |submitted_image_| could
    // be released due to reshape() or destruction.
    swap_completion_callbacks_.emplace_back(
        std::make_unique<CancelableSwapCompletionCallback>(base::BindOnce(
            &SkiaOutputDeviceBufferQueue::DoFinishSwapBuffers,
            base::Unretained(this), image_size_, std::move(latency_info),
            submitted_image_->GetWeakPtr(), std::move(committed_overlays_))));
    gl_surface_->CommitOverlayPlanesAsync(
        swap_completion_callbacks_.back()->callback(), std::move(feedback));
  } else {
    DoFinishSwapBuffers(
        image_size_, std::move(latency_info), submitted_image_->GetWeakPtr(),
        std::move(committed_overlays_),
        gl_surface_->CommitOverlayPlanes(std::move(feedback)), nullptr);
  }
  committed_overlays_.clear();
  std::swap(committed_overlays_, pending_overlays_);
}

void SkiaOutputDeviceBufferQueue::DoFinishSwapBuffers(
    const gfx::Size& size,
    std::vector<ui::LatencyInfo> latency_info,
    const base::WeakPtr<Image>& image,
    std::vector<OverlayData> overlays,
    gfx::SwapResult result,
    std::unique_ptr<gfx::GpuFence> gpu_fence) {
  DCHECK(!gpu_fence);

  FinishSwapBuffers(result, size, latency_info);
  PageFlipComplete(image.get());
}

bool SkiaOutputDeviceBufferQueue::Reshape(const gfx::Size& size,
                                          float device_scale_factor,
                                          const gfx::ColorSpace& color_space,
                                          gfx::BufferFormat format,
                                          gfx::OverlayTransform transform) {
  if (!gl_surface_->Resize(size, device_scale_factor, color_space,
                           gfx::AlphaBitsForBufferFormat(format))) {
    DLOG(ERROR) << "Failed to resize.";
    return false;
  }

  color_space_ = color_space;
  image_size_ = size;
  FreeAllSurfaces();

  for (int i = 0; i < capabilities_.max_frames_pending + 1; ++i) {
    auto image = std::make_unique<Image>(
        &shared_image_factory_, shared_image_representation_factory_.get());
    if (!image->Initialize(image_size_, color_space_, image_format_,
                           dependency_, shared_image_usage_)) {
      DLOG(ERROR) << "Failed to initialize image.";
      return false;
    }
    available_images_.push_back(image.get());
    images_.push_back(std::move(image));
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

}  // namespace viz
