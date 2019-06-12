// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_surface_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_util.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/resource_metadata.h"
#include "components/viz/service/display_embedder/image_context.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"
#include "components/viz/service/display_embedder/skia_output_surface_impl_on_gpu.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/vulkan/buildflags.h"
#include "ui/gfx/skia_util.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_gl_api_implementation.h"

namespace viz {

namespace {

sk_sp<SkPromiseImageTexture> Fulfill(void* texture_context) {
  DCHECK(texture_context);
  auto* image_context = static_cast<ImageContext*>(texture_context);
  return image_context->promise_image_texture;
}

void DoNothing(void* texture_context) {}

template <typename... Args>
void PostAsyncTask(SkiaOutputSurfaceDependency* dependency,
                   const base::RepeatingCallback<void(Args...)>& callback,
                   Args... args) {
  dependency->PostTaskToClientThread(base::BindOnce(callback, args...));
}

template <typename... Args>
base::RepeatingCallback<void(Args...)> CreateSafeCallback(
    SkiaOutputSurfaceDependency* dependency,
    const base::RepeatingCallback<void(Args...)>& callback) {
  DCHECK(dependency);
  return base::BindRepeating(&PostAsyncTask<Args...>, dependency, callback);
}

}  // namespace

SkiaOutputSurfaceImpl::SkiaOutputSurfaceImpl(
    std::unique_ptr<SkiaOutputSurfaceDependency> deps,
    const RendererSettings& renderer_settings)
    : dependency_(std::move(deps)),
      is_using_vulkan_(dependency_->IsUsingVulkan()),
      renderer_settings_(renderer_settings),
      weak_ptr_factory_(this) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

SkiaOutputSurfaceImpl::~SkiaOutputSurfaceImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  recorder_.reset();

  std::vector<std::unique_ptr<ImageContext>> image_contexts;
  image_contexts.reserve(promise_image_cache_.size());
  for (auto& id_and_image_context : promise_image_cache_) {
    id_and_image_context.second->image = nullptr;
    image_contexts.push_back(std::move(id_and_image_context.second));
  }

  std::vector<std::unique_ptr<ImageContext>> render_pass_image_contexts;
  render_pass_image_contexts.reserve(render_pass_image_cache_.size());
  for (auto& id_and_image_context : render_pass_image_cache_) {
    id_and_image_context.second->image = nullptr;
    render_pass_image_contexts.push_back(
        std::move(id_and_image_context.second));
  }

  base::WaitableEvent event;
  auto callback = base::BindOnce(
      [](std::vector<std::unique_ptr<ImageContext>> images,
         std::vector<std::unique_ptr<ImageContext>> render_passes,
         std::unique_ptr<SkiaOutputSurfaceImplOnGpu> impl_on_gpu,
         base::WaitableEvent* event) {
        if (!images.empty())
          impl_on_gpu->ReleaseSkImages(std::move(images));
        if (!render_passes.empty())
          impl_on_gpu->RemoveRenderPassResource(std::move(render_passes));
        impl_on_gpu = nullptr;
        event->Signal();
      },
      std::move(image_contexts), std::move(render_pass_image_contexts),
      std::move(impl_on_gpu_), &event);
  ScheduleGpuTask(std::move(callback), std::vector<gpu::SyncToken>());
  event.Wait();
}

void SkiaOutputSurfaceImpl::BindToClient(OutputSurfaceClient* client) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  SkiaOutputSurfaceBase::BindToClient(client);
  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  auto callback = base::BindOnce(&SkiaOutputSurfaceImpl::InitializeOnGpuThread,
                                 base::Unretained(this), &event);
  ScheduleGpuTask(std::move(callback), std::vector<gpu::SyncToken>());
  event.Wait();
}

void SkiaOutputSurfaceImpl::SetDrawRectangle(const gfx::Rect& draw_rectangle) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto callback =
      base::BindOnce(&SkiaOutputSurfaceImplOnGpu::SetDrawRectangle,
                     base::Unretained(impl_on_gpu_.get()), draw_rectangle);
  ScheduleGpuTask(std::move(callback), {});
}

void SkiaOutputSurfaceImpl::EnsureBackbuffer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // impl_on_gpu_ is released on the GPU thread by a posted task from
  // SkiaOutputSurfaceImpl::dtor. So it is safe to use base::Unretained.
  auto callback = base::BindOnce(&SkiaOutputSurfaceImplOnGpu::EnsureBackbuffer,
                                 base::Unretained(impl_on_gpu_.get()));
  ScheduleGpuTask(std::move(callback), std::vector<gpu::SyncToken>());
}

void SkiaOutputSurfaceImpl::DiscardBackbuffer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // impl_on_gpu_ is released on the GPU thread by a posted task from
  // SkiaOutputSurfaceImpl::dtor. So it is safe to use base::Unretained.
  auto callback = base::BindOnce(&SkiaOutputSurfaceImplOnGpu::DiscardBackbuffer,
                                 base::Unretained(impl_on_gpu_.get()));
  ScheduleGpuTask(std::move(callback), std::vector<gpu::SyncToken>());
}

void SkiaOutputSurfaceImpl::Reshape(const gfx::Size& size,
                                    float device_scale_factor,
                                    const gfx::ColorSpace& color_space,
                                    bool has_alpha,
                                    bool use_stencil) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (initialize_waitable_event_) {
    initialize_waitable_event_->Wait();
    initialize_waitable_event_ = nullptr;
  }

  SkSurfaceCharacterization* characterization = nullptr;
  if (characterization_.isValid()) {
    // TODO(weiliang): support color space. https://crbug.com/795132
    characterization_ =
        characterization_.createResized(size.width(), size.height());
  } else {
    characterization = &characterization_;
    initialize_waitable_event_ = std::make_unique<base::WaitableEvent>(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
  }

  // impl_on_gpu_ is released on the GPU thread by a posted task from
  // SkiaOutputSurfaceImpl::dtor. So it is safe to use base::Unretained.
  auto callback = base::BindOnce(&SkiaOutputSurfaceImplOnGpu::Reshape,
                                 base::Unretained(impl_on_gpu_.get()), size,
                                 device_scale_factor, std::move(color_space),
                                 has_alpha, use_stencil, characterization,
                                 initialize_waitable_event_.get());
  ScheduleGpuTask(std::move(callback), std::vector<gpu::SyncToken>());
}

void SkiaOutputSurfaceImpl::SetUpdateVSyncParametersCallback(
    UpdateVSyncParametersCallback callback) {
  update_vsync_parameters_callback_ = std::move(callback);
}

SkCanvas* SkiaOutputSurfaceImpl::BeginPaintCurrentFrame() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Make sure there is no unsubmitted PaintFrame or PaintRenderPass.
  DCHECK(!recorder_);
  DCHECK_EQ(current_render_pass_id_, 0u);

  if (initialize_waitable_event_) {
    initialize_waitable_event_->Wait();
    initialize_waitable_event_ = nullptr;
  }

  DCHECK(characterization_.isValid());
  recorder_.emplace(characterization_);
  if (!renderer_settings_.show_overdraw_feedback)
    return recorder_->getCanvas();

  DCHECK(!overdraw_surface_recorder_);
  DCHECK(renderer_settings_.show_overdraw_feedback);

  SkSurfaceCharacterization characterization = CreateSkSurfaceCharacterization(
      gfx::Size(characterization_.width(), characterization_.height()),
      BGRA_8888, false /* mipmap */, characterization_.refColorSpace());
  overdraw_surface_recorder_.emplace(characterization);
  overdraw_canvas_.emplace((overdraw_surface_recorder_->getCanvas()));

  nway_canvas_.emplace(characterization_.width(), characterization_.height());
  nway_canvas_->addCanvas(recorder_->getCanvas());
  nway_canvas_->addCanvas(&overdraw_canvas_.value());
  return &nway_canvas_.value();
}

sk_sp<SkImage> SkiaOutputSurfaceImpl::MakePromiseSkImage(
    const ResourceMetadata& metadata) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(recorder_);
  DCHECK(!metadata.mailbox_holder.mailbox.IsZero());

  auto& image_context = promise_image_cache_[metadata.resource_id];
  if (!image_context) {
    image_context = std::make_unique<ImageContext>(metadata);
    SkColorType color_type = ResourceFormatToClosestSkColorType(
        true /* gpu_compositing */, metadata.resource_format);

    GrBackendFormat backend_format = GetGrBackendFormatForTexture(
        metadata.resource_format, metadata.mailbox_holder.texture_target,
        metadata.ycbcr_info);
    image_context->image = recorder_->makePromiseTexture(
        backend_format, metadata.size.width(), metadata.size.height(),
        GrMipMapped::kNo, metadata.origin, color_type, metadata.alpha_type,
        image_context->color_space, Fulfill, DoNothing, DoNothing,
        image_context.get());
    DCHECK(image_context->image);
  }

  if (image_context->sync_token.HasData()) {
    resource_sync_tokens_.push_back(image_context->sync_token);
    image_context->sync_token.Clear();
  }
  images_in_current_paint_.push_back(image_context.get());

  return image_context->image;
}

sk_sp<SkImage> SkiaOutputSurfaceImpl::MakePromiseSkImageFromYUV(
    const std::vector<ResourceMetadata>& metadatas,
    SkYUVColorSpace yuv_color_space,
    sk_sp<SkColorSpace> dst_color_space,
    bool has_alpha) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(recorder_);
  DCHECK((has_alpha && (metadatas.size() == 3 || metadatas.size() == 4)) ||
         (!has_alpha && (metadatas.size() == 2 || metadatas.size() == 3)));

  SkYUVAIndex indices[4];
  PrepareYUVATextureIndices(metadatas, has_alpha, indices);

  GrBackendFormat formats[4] = {};
  SkISize yuva_sizes[4] = {};
  SkDeferredDisplayListRecorder::PromiseImageTextureContext
      texture_contexts[4] = {};
  for (size_t i = 0; i < metadatas.size(); ++i) {
    const auto& metadata = metadatas[i];
    DCHECK(metadata.origin == kTopLeft_GrSurfaceOrigin);
    formats[i] = GetGrBackendFormatForTexture(
        metadata.resource_format, metadata.mailbox_holder.texture_target);
    yuva_sizes[i].set(metadata.size.width(), metadata.size.height());
    auto& image_context = promise_image_cache_[metadata.resource_id];
    if (!image_context) {
      // color_space is ignored by makeYUVAPromiseTexture below. Passing nullptr
      // removes some LOG spam.
      image_context = std::make_unique<ImageContext>(
          metadata.mailbox_holder.mailbox, metadata.size,
          metadata.resource_format, nullptr /* color_space */,
          metadata.alpha_type, metadata.origin,
          metadata.mailbox_holder.sync_token);
    }
    if (image_context->sync_token.HasData()) {
      resource_sync_tokens_.push_back(image_context->sync_token);
      image_context->sync_token.Clear();
    }
    images_in_current_paint_.push_back(image_context.get());
    texture_contexts[i] = image_context.get();
  }

  auto image = recorder_->makeYUVAPromiseTexture(
      yuv_color_space, formats, yuva_sizes, indices, yuva_sizes[0].width(),
      yuva_sizes[0].height(), kTopLeft_GrSurfaceOrigin, dst_color_space,
      Fulfill, DoNothing, DoNothing, texture_contexts);
  DCHECK(image);
  return image;
}

void SkiaOutputSurfaceImpl::ReleaseCachedResources(
    const std::vector<ResourceId>& ids) {
  if (ids.empty())
    return;

  std::vector<std::unique_ptr<ImageContext>> image_contexts;
  for (auto id : ids) {
    auto it = promise_image_cache_.find(id);
    if (it == promise_image_cache_.end())
      continue;
    auto& image_context = it->second;
    image_context->image = nullptr;
    image_contexts.push_back(std::move(image_context));
    promise_image_cache_.erase(it);
  }

  if (image_contexts.empty())
    return;

  // impl_on_gpu_ is released on the GPU thread by a posted task from
  // SkiaOutputSurfaceImpl::dtor. So it is safe to use base::Unretained.
  auto callback = base::BindOnce(&SkiaOutputSurfaceImplOnGpu::ReleaseSkImages,
                                 base::Unretained(impl_on_gpu_.get()),
                                 std::move(image_contexts));
  ScheduleGpuTask(std::move(callback), std::vector<gpu::SyncToken>());
}

void SkiaOutputSurfaceImpl::SkiaSwapBuffers(OutputSurfaceFrame frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!recorder_);
  // impl_on_gpu_ is released on the GPU thread by a posted task from
  // SkiaOutputSurfaceImpl::dtor. So it is safe to use base::Unretained.
  auto callback =
      base::BindOnce(&SkiaOutputSurfaceImplOnGpu::SwapBuffers,
                     base::Unretained(impl_on_gpu_.get()), std::move(frame));
  ScheduleGpuTask(std::move(callback), std::vector<gpu::SyncToken>());
}

SkCanvas* SkiaOutputSurfaceImpl::BeginPaintRenderPass(
    const RenderPassId& id,
    const gfx::Size& surface_size,
    ResourceFormat format,
    bool mipmap,
    sk_sp<SkColorSpace> color_space) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Make sure there is no unsubmitted PaintFrame or PaintRenderPass.
  DCHECK(!recorder_);
  DCHECK_EQ(current_render_pass_id_, 0u);
  DCHECK(resource_sync_tokens_.empty());

  current_render_pass_id_ = id;

  SkSurfaceCharacterization characterization = CreateSkSurfaceCharacterization(
      surface_size, format, mipmap, std::move(color_space));
  recorder_.emplace(characterization);
  return recorder_->getCanvas();
}

gpu::SyncToken SkiaOutputSurfaceImpl::SubmitPaint(
    base::OnceClosure on_finished) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(recorder_);

  // If current_render_pass_id_ is not 0, we are painting a render pass.
  // Otherwise we are painting a frame.
  bool painting_render_pass = current_render_pass_id_ != 0;

  gpu::SyncToken sync_token(
      gpu::CommandBufferNamespace::VIZ_SKIA_OUTPUT_SURFACE,
      impl_on_gpu_->command_buffer_id(), ++sync_fence_release_);
  sync_token.SetVerifyFlush();

  auto ddl = recorder_->detach();
  DCHECK(ddl);
  recorder_.reset();
  std::unique_ptr<SkDeferredDisplayList> overdraw_ddl;
  if (renderer_settings_.show_overdraw_feedback && !painting_render_pass) {
    overdraw_ddl = overdraw_surface_recorder_->detach();
    DCHECK(overdraw_ddl);
    overdraw_canvas_.reset();
    nway_canvas_.reset();
    overdraw_surface_recorder_.reset();
  }

  // impl_on_gpu_ is released on the GPU thread by a posted task from
  // SkiaOutputSurfaceImpl::dtor. So it is safe to use base::Unretained.
  base::OnceCallback<void()> callback;
  if (painting_render_pass) {
    auto it = render_pass_image_cache_.find(current_render_pass_id_);
    if (it != render_pass_image_cache_.end()) {
      // We are going to overwrite the render pass, so we need reset the
      // image_context, so a new promise image will be created when the
      // MakePromiseSkImageFromRenderPass() is called.
      it->second->image = nullptr;
    }
    DCHECK(!on_finished);
    callback = base::BindOnce(
        &SkiaOutputSurfaceImplOnGpu::FinishPaintRenderPass,
        base::Unretained(impl_on_gpu_.get()), current_render_pass_id_,
        std::move(ddl), std::move(images_in_current_paint_),
        resource_sync_tokens_, sync_fence_release_);
  } else {
    callback = base::BindOnce(
        &SkiaOutputSurfaceImplOnGpu::FinishPaintCurrentFrame,
        base::Unretained(impl_on_gpu_.get()), std::move(ddl),
        std::move(overdraw_ddl), std::move(images_in_current_paint_),
        resource_sync_tokens_, sync_fence_release_, std::move(on_finished));
  }
  images_in_current_paint_.clear();
  ScheduleGpuTask(std::move(callback), std::move(resource_sync_tokens_));
  current_render_pass_id_ = 0;
  return sync_token;
}

sk_sp<SkImage> SkiaOutputSurfaceImpl::MakePromiseSkImageFromRenderPass(
    const RenderPassId& id,
    const gfx::Size& size,
    ResourceFormat format,
    bool mipmap,
    sk_sp<SkColorSpace> color_space) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(recorder_);

  auto& image_context = render_pass_image_cache_[id];
  if (!image_context) {
    image_context = std::make_unique<ImageContext>(id, size, format, mipmap,
                                                   std::move(color_space));
  }
  if (!image_context->image) {
    SkColorType color_type =
        ResourceFormatToClosestSkColorType(true /* gpu_compositing */, format);
    GrBackendFormat backend_format =
        GetGrBackendFormatForTexture(format, GL_TEXTURE_2D);
    image_context->image = recorder_->makePromiseTexture(
        backend_format, image_context->size.width(),
        image_context->size.height(), image_context->mipmap,
        image_context->origin, color_type, image_context->alpha_type,
        image_context->color_space, Fulfill, DoNothing, DoNothing,
        image_context.get());
    DCHECK(image_context->image);
  }
  images_in_current_paint_.push_back(image_context.get());
  return image_context->image;
}

void SkiaOutputSurfaceImpl::RemoveRenderPassResource(
    std::vector<RenderPassId> ids) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!ids.empty());

  std::vector<std::unique_ptr<ImageContext>> image_contexts;
  image_contexts.reserve(ids.size());
  for (const auto id : ids) {
    auto it = render_pass_image_cache_.find(id);
    // If the render pass was only used for a copy request, there won't be a
    // matching entry in |render_pass_image_cache_|.
    if (it != render_pass_image_cache_.end()) {
      it->second->image = nullptr;
      image_contexts.push_back(std::move(it->second));
      render_pass_image_cache_.erase(it);
    }
  }

  // impl_on_gpu_ is released on the GPU thread by a posted task from
  // SkiaOutputSurfaceImpl::dtor. So it is safe to use base::Unretained.
  if (!image_contexts.empty()) {
    auto callback = base::BindOnce(
        &SkiaOutputSurfaceImplOnGpu::RemoveRenderPassResource,
        base::Unretained(impl_on_gpu_.get()), std::move(image_contexts));
    ScheduleGpuTask(std::move(callback), std::vector<gpu::SyncToken>());
  }
}

void SkiaOutputSurfaceImpl::CopyOutput(
    RenderPassId id,
    const copy_output::RenderPassGeometry& geometry,
    const gfx::ColorSpace& color_space,
    std::unique_ptr<CopyOutputRequest> request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!request->has_result_task_runner())
    request->set_result_task_runner(base::ThreadTaskRunnerHandle::Get());

  auto callback = base::BindOnce(&SkiaOutputSurfaceImplOnGpu::CopyOutput,
                                 base::Unretained(impl_on_gpu_.get()), id,
                                 geometry, color_space, std::move(request));
  ScheduleGpuTask(std::move(callback), std::vector<gpu::SyncToken>());
}

void SkiaOutputSurfaceImpl::SetCapabilitiesForTesting(
    bool flipped_output_surface) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(impl_on_gpu_);
  capabilities_.flipped_output_surface = flipped_output_surface;
  auto callback =
      base::BindOnce(&SkiaOutputSurfaceImplOnGpu::SetCapabilitiesForTesting,
                     base::Unretained(impl_on_gpu_.get()), capabilities_);
  ScheduleGpuTask(std::move(callback), std::vector<gpu::SyncToken>());
}

void SkiaOutputSurfaceImpl::InitializeOnGpuThread(base::WaitableEvent* event) {
  base::Optional<base::ScopedClosureRunner> scoped_runner;
  if (event) {
    scoped_runner.emplace(
        base::BindOnce(&base::WaitableEvent::Signal, base::Unretained(event)));
  }

  auto did_swap_buffer_complete_callback = base::BindRepeating(
      &SkiaOutputSurfaceImpl::DidSwapBuffersComplete, weak_ptr_);
  did_swap_buffer_complete_callback =
      CreateSafeCallback(dependency_.get(), did_swap_buffer_complete_callback);
  auto buffer_presented_callback =
      base::BindRepeating(&SkiaOutputSurfaceImpl::BufferPresented, weak_ptr_);
  buffer_presented_callback =
      CreateSafeCallback(dependency_.get(), buffer_presented_callback);
  auto context_lost_callback =
      base::BindRepeating(&SkiaOutputSurfaceImpl::ContextLost, weak_ptr_);
  context_lost_callback =
      CreateSafeCallback(dependency_.get(), context_lost_callback);
  impl_on_gpu_ = std::make_unique<SkiaOutputSurfaceImplOnGpu>(
      dependency_.get(), renderer_settings_, did_swap_buffer_complete_callback,
      buffer_presented_callback, context_lost_callback);
  capabilities_ = impl_on_gpu_->capabilities();
}

SkSurfaceCharacterization
SkiaOutputSurfaceImpl::CreateSkSurfaceCharacterization(
    const gfx::Size& surface_size,
    ResourceFormat format,
    bool mipmap,
    sk_sp<SkColorSpace> color_space) {
  auto gr_context_thread_safe = impl_on_gpu_->GetGrContextThreadSafeProxy();
  constexpr uint32_t flags = 0;
  // LegacyFontHost will get LCD text and skia figures out what type to use.
  SkSurfaceProps surface_props(flags, SkSurfaceProps::kLegacyFontHost_InitType);
  int msaa_sample_count = 0;
  SkColorType color_type =
      ResourceFormatToClosestSkColorType(true /* gpu_compositing */, format);
  SkImageInfo image_info =
      SkImageInfo::Make(surface_size.width(), surface_size.height(), color_type,
                        kPremul_SkAlphaType, std::move(color_space));

  // TODO(penghuang): Figure out how to choose the right size.
  constexpr size_t kCacheMaxResourceBytes = 90 * 1024 * 1024;

  GrBackendFormat backend_format;
  if (!is_using_vulkan_) {
    const auto* version_info = impl_on_gpu_->gl_version_info();
    unsigned int texture_storage_format = TextureStorageFormat(format);
    backend_format = GrBackendFormat::MakeGL(
        gl::GetInternalFormat(version_info, texture_storage_format),
        GL_TEXTURE_2D);
  } else {
#if BUILDFLAG(ENABLE_VULKAN)
    backend_format = GrBackendFormat::MakeVk(ToVkFormat(format));
#else
    NOTREACHED();
#endif
  }
  auto characterization = gr_context_thread_safe->createCharacterization(
      kCacheMaxResourceBytes, image_info, backend_format, msaa_sample_count,
      kTopLeft_GrSurfaceOrigin, surface_props, mipmap);
  DCHECK(characterization.isValid());
  return characterization;
}

void SkiaOutputSurfaceImpl::DidSwapBuffersComplete(
    gpu::SwapBuffersCompleteParams params,
    const gfx::Size& pixel_size) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(client_);

  if (!params.texture_in_use_responses.empty())
    client_->DidReceiveTextureInUseResponses(params.texture_in_use_responses);
  if (!params.ca_layer_params.is_empty)
    client_->DidReceiveCALayerParams(params.ca_layer_params);
  client_->DidReceiveSwapBuffersAck();
  if (needs_swap_size_notifications_)
    client_->DidSwapWithSize(pixel_size);
}

void SkiaOutputSurfaceImpl::BufferPresented(
    const gfx::PresentationFeedback& feedback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(client_);
  client_->DidReceivePresentationFeedback(feedback);
  if (update_vsync_parameters_callback_ &&
      feedback.flags & gfx::PresentationFeedback::kVSync) {
    // TODO(brianderson): We should not be receiving 0 intervals.
    update_vsync_parameters_callback_.Run(
        feedback.timestamp, feedback.interval.is_zero()
                                ? BeginFrameArgs::DefaultInterval()
                                : feedback.interval);
  }
}

void SkiaOutputSurfaceImpl::ScheduleGpuTask(
    base::OnceClosure callback,
    std::vector<gpu::SyncToken> sync_tokens) {
  dependency_->ScheduleGpuTask(std::move(callback), std::move(sync_tokens));
}

GrBackendFormat SkiaOutputSurfaceImpl::GetGrBackendFormatForTexture(
    ResourceFormat resource_format,
    uint32_t gl_texture_target,
    base::Optional<gpu::VulkanYCbCrInfo> ycbcr_info) {
  if (!is_using_vulkan_) {
    DCHECK(!ycbcr_info);
    // Convert internal format from GLES2 to platform GL.
    const auto* version_info = impl_on_gpu_->gl_version_info();
    unsigned int texture_storage_format = TextureStorageFormat(resource_format);
    // Switch to format supported by Skia.
    if (texture_storage_format == GL_LUMINANCE16F_EXT)
      texture_storage_format = GL_R16F_EXT;
    return GrBackendFormat::MakeGL(
        gl::GetInternalFormat(version_info, texture_storage_format),
        gl_texture_target);
  } else {
#if BUILDFLAG(ENABLE_VULKAN)
    if (!ycbcr_info)
      return GrBackendFormat::MakeVk(ToVkFormat(resource_format));

    GrVkYcbcrConversionInfo fYcbcrConversionInfo(
        static_cast<VkSamplerYcbcrModelConversion>(
            ycbcr_info->suggested_ycbcr_model),
        static_cast<VkSamplerYcbcrRange>(ycbcr_info->suggested_ycbcr_range),
        static_cast<VkChromaLocation>(ycbcr_info->suggested_xchroma_offset),
        static_cast<VkChromaLocation>(ycbcr_info->suggested_ychroma_offset),
        VK_FILTER_LINEAR,  // VkFilter
        0,                 // VkBool32 forceExplicitReconstruction,
        ycbcr_info->external_format,
        static_cast<VkFormatFeatureFlags>(ycbcr_info->format_features));
    return GrBackendFormat::MakeVk(fYcbcrConversionInfo);
#else
    NOTREACHED();
    return GrBackendFormat();
#endif
  }
}

}  // namespace viz
