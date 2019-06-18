// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_surface_impl_non_ddl.h"

#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/synchronization/waitable_event.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/resource_metadata.h"
#include "components/viz/service/display_embedder/image_context.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/command_buffer/service/texture_base.h"
#include "gpu/vulkan/buildflags.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "ui/gfx/skia_util.h"
#include "ui/gl/color_space_utils.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_version_info.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "third_party/skia/src/gpu/vk/GrVkSecondaryCBDrawContext.h"
#endif

namespace viz {

namespace {

scoped_refptr<gpu::SyncPointClientState> CreateSyncPointClientState(
    gpu::SyncPointManager* sync_point_manager,
    gpu::SequenceId sequence_id) {
  static uint64_t next_command_buffer_id = 0u;
  auto command_buffer_id =
      gpu::CommandBufferId::FromUnsafeValue(++next_command_buffer_id);
  return sync_point_manager->CreateSyncPointClientState(
      gpu::CommandBufferNamespace::VIZ_SKIA_OUTPUT_SURFACE_NON_DDL,
      command_buffer_id, sequence_id);
}

std::unique_ptr<gpu::SharedImageRepresentationFactory>
CreateSharedImageRepresentationFactory(gpu::SharedImageManager* manager) {
  if (!manager)
    return nullptr;
  // TODO(https://crbug.com/899905): Use a real MemoryTracker, not nullptr.
  return std::make_unique<gpu::SharedImageRepresentationFactory>(
      manager, nullptr /* tracker */);
}

}  // namespace

SkiaOutputSurfaceImplNonDDL::SkiaOutputSurfaceImplNonDDL(
    scoped_refptr<gl::GLSurface> gl_surface,
    scoped_refptr<gpu::SharedContextState> shared_context_state,
    gpu::MailboxManager* mailbox_manager,
    gpu::SharedImageManager* shared_image_manager,
    gpu::SyncPointManager* sync_point_manager,
    bool need_swapbuffers_ack)
    : gl_surface_(std::move(gl_surface)),
      shared_context_state_(std::move(shared_context_state)),
      mailbox_manager_(mailbox_manager),
      sir_factory_(
          CreateSharedImageRepresentationFactory(shared_image_manager)),
      sync_point_order_data_(sync_point_manager->CreateSyncPointOrderData()),
      sync_point_client_state_(
          CreateSyncPointClientState(sync_point_manager,
                                     sync_point_order_data_->sequence_id())),
      need_swapbuffers_ack_(need_swapbuffers_ack),
      weak_ptr_factory_(this) {}

SkiaOutputSurfaceImplNonDDL::~SkiaOutputSurfaceImplNonDDL() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void SkiaOutputSurfaceImplNonDDL::EnsureBackbuffer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NOTIMPLEMENTED();
}

void SkiaOutputSurfaceImplNonDDL::DiscardBackbuffer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NOTIMPLEMENTED();
}

void SkiaOutputSurfaceImplNonDDL::Reshape(const gfx::Size& size,
                                          float device_scale_factor,
                                          const gfx::ColorSpace& color_space,
                                          bool has_alpha,
                                          bool use_stencil) {
  reshape_surface_size_ = size;
  reshape_device_scale_factor_ = device_scale_factor;
  reshape_color_space_ = color_space;
  reshape_has_alpha_ = has_alpha;
  reshape_use_stencil_ = use_stencil;

  if (shared_context_state_->GrContextIsVulkan()) {
    auto* context_provider = shared_context_state_->vk_context_provider();
    DCHECK(context_provider->GetGrSecondaryCBDrawContext());
  } else if (shared_context_state_->GrContextIsGL()) {
    gl::GLSurface::ColorSpace surface_color_space =
        gl::ColorSpaceUtils::GetGLSurfaceColorSpace(color_space);
    gl_surface_->Resize(size, device_scale_factor, surface_color_space,
                        has_alpha);

    backing_framebuffer_object_ = gl_surface_->GetBackingFramebufferObject();

    SkSurfaceProps surface_props =
        SkSurfaceProps(0, SkSurfaceProps::kLegacyFontHost_InitType);

    GrGLFramebufferInfo framebuffer_info;
    framebuffer_info.fFBOID = backing_framebuffer_object_;
    framebuffer_info.fFormat = GL_RGBA8;

    GrBackendRenderTarget render_target(size.width(), size.height(), 0, 8,
                                        framebuffer_info);

    sk_surface_ = SkSurface::MakeFromBackendRenderTarget(
        gr_context(), render_target, kBottomLeft_GrSurfaceOrigin,
        kRGBA_8888_SkColorType, color_space.ToSkColorSpace(), &surface_props);
    DCHECK(sk_surface_);
  } else {
    NOTIMPLEMENTED();
  }
}

SkCanvas* SkiaOutputSurfaceImplNonDDL::BeginPaintCurrentFrame() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(current_render_pass_id_, 0u);
  DCHECK(!scoped_gpu_task_);
  scoped_gpu_task_.emplace(sync_point_order_data_.get());

  if (shared_context_state_->GrContextIsVulkan()) {
#if BUILDFLAG(ENABLE_VULKAN)
    DCHECK(!draw_context_);
    draw_context_ = shared_context_state_->vk_context_provider()
                        ->GetGrSecondaryCBDrawContext();
    DCHECK(draw_context_);
    return draw_context_->getCanvas();
#else
    NOTREACHED();
    return nullptr;
#endif
  } else {
    DCHECK(sk_surface_);
    // If FBO is changed, we need call Reshape() to recreate |sk_surface_|.
    if (backing_framebuffer_object_ !=
        gl_surface_->GetBackingFramebufferObject()) {
      Reshape(reshape_surface_size_, reshape_device_scale_factor_,
              reshape_color_space_, reshape_has_alpha_, reshape_use_stencil_);
    }
    sk_current_surface_ = sk_surface_.get();
    return sk_current_surface_->getCanvas();
  }
}

sk_sp<SkImage> SkiaOutputSurfaceImplNonDDL::MakePromiseSkImage(
    const ResourceMetadata& metadata) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (metadata.mailbox_holder.mailbox.IsSharedImage() && sir_factory_) {
    auto& image_context = promise_image_cache_[metadata.resource_id];
    if (!image_context)
      image_context = MakeSkImageFromSharedImage(metadata);
    if (image_context) {
      if (!image_context->representation_is_being_accessed) {
        std::vector<GrBackendSemaphore> begin_semaphores;
        auto promise_image_texture =
            image_context->representation->BeginReadAccess(
                &begin_semaphores, &pending_semaphores_);
        // The image has been created and cached. It is too late to tell skia
        // the backing of the cached image is not accessible right now, so crash
        // for now.
        // TODO(penghuang): find a way to notify skia.
        CHECK(promise_image_texture);
        image_context->representation_is_being_accessed = true;
        WaitSemaphores(std::move(begin_semaphores));
      }
      images_in_current_paint_.push_back(image_context.get());
    }
    return image_context ? image_context->image : nullptr;
  }

  GrBackendTexture backend_texture;
  if (!GetGrBackendTexture(metadata, &backend_texture)) {
    DLOG(ERROR) << "Failed to GetGrBackendTexture from mailbox.";
    return nullptr;
  }

  auto sk_color_type = ResourceFormatToClosestSkColorType(
      true /* gpu_compositing */, metadata.resource_format);
  return SkImage::MakeFromTexture(
      gr_context(), backend_texture, kTopLeft_GrSurfaceOrigin, sk_color_type,
      metadata.alpha_type, metadata.color_space.ToSkColorSpace());
}

sk_sp<SkImage> SkiaOutputSurfaceImplNonDDL::MakePromiseSkImageFromYUV(
    const std::vector<ResourceMetadata>& metadatas,
    SkYUVColorSpace yuv_color_space,
    sk_sp<SkColorSpace> dst_color_space,
    bool has_alpha) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK((has_alpha && (metadatas.size() == 3 || metadatas.size() == 4)) ||
         (!has_alpha && (metadatas.size() == 2 || metadatas.size() == 3)));

  SkYUVAIndex indices[4] = {};
  PrepareYUVATextureIndices(metadatas, has_alpha, indices);

  GrBackendTexture yuva_textures[4] = {};
  for (size_t i = 0; i < metadatas.size(); ++i) {
    const auto& metadata = metadatas[i];
    if (!GetGrBackendTexture(metadata, &yuva_textures[i]))
      DLOG(ERROR) << "Failed to GetGrBackendTexture from a mailbox.";
  }

  return SkImage::MakeFromYUVATextures(
      gr_context(), yuv_color_space, yuva_textures, indices,
      SkISize::Make(yuva_textures[0].width(), yuva_textures[1].height()),
      kTopLeft_GrSurfaceOrigin, dst_color_space);
}

void SkiaOutputSurfaceImplNonDDL::ReleaseCachedResources(
    const std::vector<ResourceId>& ids) {
  if (ids.empty())
    return;
  for (auto id : ids) {
    auto it = promise_image_cache_.find(id);
    if (it == promise_image_cache_.end())
      continue;
    it->second->image = nullptr;
    promise_image_cache_.erase(it);
  }
}

void SkiaOutputSurfaceImplNonDDL::SkiaSwapBuffers(OutputSurfaceFrame frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  gfx::SwapTimings swap_timings;
  swap_timings.swap_start = base::TimeTicks::Now();
  gl_surface_->SwapBuffers(
      base::BindOnce(&SkiaOutputSurfaceImplNonDDL::BufferPresented,
                     weak_ptr_factory_.GetWeakPtr()));
  swap_timings.swap_end = base::TimeTicks::Now();
  if (need_swapbuffers_ack_)
    client_->DidReceiveSwapBuffersAck(swap_timings);
}

SkCanvas* SkiaOutputSurfaceImplNonDDL::BeginPaintRenderPass(
    const RenderPassId& id,
    const gfx::Size& surface_size,
    ResourceFormat format,
    bool mipmap,
    sk_sp<SkColorSpace> color_space) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Make sure there is no unsubmitted PaintFrame or PaintRenderPass.
  DCHECK_EQ(current_render_pass_id_, 0u);
  DCHECK(!scoped_gpu_task_);
  scoped_gpu_task_.emplace(sync_point_order_data_.get());
  current_render_pass_id_ = id;
  auto& sk_surface = offscreen_sk_surfaces_[id];

  if (!sk_surface) {
    SkColorType color_type =
        ResourceFormatToClosestSkColorType(true /* gpu_compositing */, format);
    SkImageInfo image_info = SkImageInfo::Make(
        surface_size.width(), surface_size.height(), color_type,
        kPremul_SkAlphaType, std::move(color_space));
    sk_surface =
        SkSurface::MakeRenderTarget(gr_context(), SkBudgeted::kNo, image_info);
  }

  sk_current_surface_ = sk_surface.get();
  return sk_current_surface_->getCanvas();
}

gpu::SyncToken SkiaOutputSurfaceImplNonDDL::SubmitPaint(
    base::OnceClosure on_finished) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // To make sure sync_token is valid, we need make sure we are processing a gpu
  // task.
  DCHECK(scoped_gpu_task_);
  gpu::SyncToken sync_token(
      gpu::CommandBufferNamespace::VIZ_SKIA_OUTPUT_SURFACE_NON_DDL,
      sync_point_client_state_->command_buffer_id(), ++sync_fence_release_);
  sync_token.SetVerifyFlush();

  if (!sk_current_surface_) {
#if BUILDFLAG(ENABLE_VULKAN)
    DCHECK(shared_context_state_->GrContextIsVulkan());
    DCHECK(draw_context_);
    draw_context_->flush();
    draw_context_ = nullptr;
    // Enqueue vk_semaphores which will be signalled when SecondaryCB is
    // executed and finished.
    std::vector<VkSemaphore> vk_semaphores;
    vk_semaphores.reserve(pending_semaphores_.size());
    for (const auto& semaphore : pending_semaphores_) {
      DCHECK(semaphore.vkSemaphore() != VK_NULL_HANDLE);
      vk_semaphores.push_back(semaphore.vkSemaphore());
    }
    pending_semaphores_.clear();
    auto* vk_context_provider = shared_context_state_->vk_context_provider();
    vk_context_provider->EnqueueSecondaryCBSemaphores(std::move(vk_semaphores));
    // Enqueue FinishPaint which will be executed when the SecondaryCB is
    // submitted and all enqueued semaphores have been submitted for signalling.
    // WebView will not destroy OutputSurface when DrawVk is pending
    // (PostDrawVK is not called), so it is safe to use base::Unretaied() here.
    vk_context_provider->EnqueueSecondaryCBPostSubmitTask(
        base::BindOnce(&SkiaOutputSurfaceImplNonDDL::FinishPaint,
                       base::Unretained(this), sync_fence_release_));
    return sync_token;
#else
    NOTREACHED();
    return gpu::SyncToken();
#endif
  }

  auto access = current_render_pass_id_ == 0
                    ? SkSurface::BackendSurfaceAccess::kPresent
                    : SkSurface::BackendSurfaceAccess::kNoAccess;
  GrFlushInfo flush_info = {
      .fFlags = kNone_GrFlushFlags,
      .fNumSemaphores = pending_semaphores_.size(),
      .fSignalSemaphores = pending_semaphores_.data(),
  };

  gpu::AddVulkanCleanupTaskForSkiaFlush(
      shared_context_state_->vk_context_provider(), &flush_info);
  if (on_finished)
    gpu::AddCleanupTaskForSkiaFlush(std::move(on_finished), &flush_info);

  auto result = sk_current_surface_->flush(access, flush_info);
  DCHECK_EQ(result, GrSemaphoresSubmitted::kYes);
  pending_semaphores_.clear();
  sk_current_surface_ = nullptr;

  FinishPaint(sync_fence_release_);
  return sync_token;
}

sk_sp<SkImage> SkiaOutputSurfaceImplNonDDL::MakePromiseSkImageFromRenderPass(
    const RenderPassId& id,
    const gfx::Size& size,
    ResourceFormat format,
    bool mipmap,
    sk_sp<SkColorSpace> color_space) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto it = offscreen_sk_surfaces_.find(id);
  DCHECK(it != offscreen_sk_surfaces_.end());
  return it->second->makeImageSnapshot();
}

void SkiaOutputSurfaceImplNonDDL::RemoveRenderPassResource(
    std::vector<RenderPassId> ids) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!ids.empty());

  for (const auto& id : ids) {
    auto it = offscreen_sk_surfaces_.find(id);
    DCHECK(it != offscreen_sk_surfaces_.end());
    offscreen_sk_surfaces_.erase(it);
  }
}

void SkiaOutputSurfaceImplNonDDL::CopyOutput(
    RenderPassId id,
    const copy_output::RenderPassGeometry& geometry,
    const gfx::ColorSpace& color_space,
    std::unique_ptr<CopyOutputRequest> request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NOTIMPLEMENTED();
}

bool SkiaOutputSurfaceImplNonDDL::WaitSyncToken(
    const gpu::SyncToken& sync_token) {
  base::WaitableEvent event;
  if (!sync_point_client_state_->Wait(
          sync_token, base::BindOnce(&base::WaitableEvent::Signal,
                                     base::Unretained(&event)))) {
    return false;
  }
  event.Wait();
  return true;
}

std::unique_ptr<ImageContext>
SkiaOutputSurfaceImplNonDDL::MakeSkImageFromSharedImage(
    const ResourceMetadata& metadata) {
  auto image_context = std::make_unique<ImageContext>(metadata);
  WaitSyncToken(image_context->sync_token);
  image_context->representation = sir_factory_->ProduceSkia(
      image_context->mailbox, shared_context_state_.get());
  if (!image_context->representation) {
    DLOG(ERROR) << "Failed to make the SkImage - SharedImage mailbox not found "
                   "in SharedImageManager.";
    return nullptr;
  }

  if (!(image_context->representation->usage() &
        gpu::SHARED_IMAGE_USAGE_DISPLAY)) {
    DLOG(ERROR) << "Failed to make the SkImage - SharedImage was not created "
                   "with display usage.";
    return nullptr;
  }

  std::vector<GrBackendSemaphore> begin_semaphores;
  image_context->promise_image_texture =
      image_context->representation->BeginReadAccess(&begin_semaphores,
                                                     &pending_semaphores_);
  if (!image_context->promise_image_texture) {
    DLOG(ERROR)
        << "Failed to make the SkImage - SharedImage begin access failed.";
    return nullptr;
  }
  image_context->representation_is_being_accessed = true;
  WaitSemaphores(std::move(begin_semaphores));

  SkColorType color_type = ResourceFormatToClosestSkColorType(
      true /* gpu_compositing */, metadata.resource_format);

  image_context->image = SkImage::MakeFromTexture(
      gr_context(), image_context->promise_image_texture->backendTexture(),
      kTopLeft_GrSurfaceOrigin, color_type, image_context->alpha_type,
      image_context->color_space);

  if (!image_context->image) {
    DLOG(ERROR) << "Failed to create the SkImage";
    return nullptr;
  }
  return image_context;
}

bool SkiaOutputSurfaceImplNonDDL::GetGrBackendTexture(
    const ResourceMetadata& metadata,
    GrBackendTexture* backend_texture) {
  DCHECK(!metadata.mailbox_holder.mailbox.IsZero());
  if (WaitSyncToken(metadata.mailbox_holder.sync_token)) {
    if (shared_context_state_->GrContextIsGL()) {
      DCHECK(mailbox_manager_->UsesSync());
      mailbox_manager_->PullTextureUpdates(metadata.mailbox_holder.sync_token);
    }
  }

  auto* texture_base =
      mailbox_manager_->ConsumeTexture(metadata.mailbox_holder.mailbox);
  if (!texture_base) {
    DLOG(ERROR) << "Failed to make the SkImage";
    return false;
  }

  auto* gl_version_info =
      shared_context_state_->real_context()->GetVersionInfo();
  return gpu::GetGrBackendTexture(gl_version_info, texture_base->target(),
                                  metadata.size, texture_base->service_id(),
                                  metadata.resource_format, backend_texture);
}

void SkiaOutputSurfaceImplNonDDL::FinishPaint(uint64_t sync_fence_release) {
  DCHECK(scoped_gpu_task_);
  for (auto* image_context : images_in_current_paint_) {
    if (!image_context->representation_is_being_accessed)
      continue;
    DCHECK(image_context->representation);
    image_context->representation->EndReadAccess();
    image_context->representation_is_being_accessed = false;
  }
  images_in_current_paint_.clear();

  if (shared_context_state_->GrContextIsGL()) {
    DCHECK(mailbox_manager_->UsesSync());
    gpu::SyncToken sync_token(
        gpu::CommandBufferNamespace::VIZ_SKIA_OUTPUT_SURFACE_NON_DDL,
        sync_point_client_state_->command_buffer_id(), sync_fence_release);
    sync_token.SetVerifyFlush();
    mailbox_manager_->PushTextureUpdates(sync_token);
  }
  sync_point_client_state_->ReleaseFenceSync(sync_fence_release);
  scoped_gpu_task_.reset();
  current_render_pass_id_ = 0;
}

void SkiaOutputSurfaceImplNonDDL::BufferPresented(
    const gfx::PresentationFeedback& feedback) {
  client_->DidReceivePresentationFeedback(feedback);
}

void SkiaOutputSurfaceImplNonDDL::WaitSemaphores(
    std::vector<GrBackendSemaphore> semaphores) {
  if (semaphores.empty())
    return;
#if BUILDFLAG(ENABLE_VULKAN)
  DCHECK(sk_current_surface_ || draw_context_);
  auto result =
      sk_current_surface_
          ? sk_current_surface_->wait(semaphores.size(), semaphores.data())
          : draw_context_->wait(semaphores.size(), semaphores.data());
#else
  DCHECK(sk_current_surface_);
  auto result = sk_current_surface_->wait(semaphores.size(), semaphores.data());
#endif

  DCHECK(result);
}

SkiaOutputSurfaceImplNonDDL::ScopedGpuTask::ScopedGpuTask(
    gpu::SyncPointOrderData* sync_point_order_data)
    : sync_point_order_data_(sync_point_order_data),
      order_num_(sync_point_order_data_->GenerateUnprocessedOrderNumber()) {
  sync_point_order_data_->BeginProcessingOrderNumber(order_num_);
}

SkiaOutputSurfaceImplNonDDL::ScopedGpuTask::~ScopedGpuTask() {
  sync_point_order_data_->FinishProcessingOrderNumber(order_num_);
}

}  // namespace viz
