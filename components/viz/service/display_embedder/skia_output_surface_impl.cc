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
#include "components/viz/service/display_embedder/image_context_impl.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"
#include "components/viz/service/display_embedder/skia_output_surface_impl_on_gpu.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/ipc/single_task_sequence.h"
#include "gpu/vulkan/buildflags.h"
#include "ui/gfx/skia_util.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_gl_api_implementation.h"

namespace viz {

namespace {

sk_sp<SkPromiseImageTexture> Fulfill(void* texture_context) {
  DCHECK(texture_context);
  auto* image_context = static_cast<ImageContextImpl*>(texture_context);
  return sk_ref_sp(image_context->promise_image_texture());
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

SkiaOutputSurfaceImpl::ScopedPaint::ScopedPaint(
    SkDeferredDisplayListRecorder* root_recorder)
    : recorder_(root_recorder), render_pass_id_(0) {}

SkiaOutputSurfaceImpl::ScopedPaint::ScopedPaint(
    SkSurfaceCharacterization characterization,
    RenderPassId render_pass_id)
    : render_pass_id_(render_pass_id) {
  recorder_storage_.emplace(characterization);
  recorder_ = &recorder_storage_.value();
}

SkiaOutputSurfaceImpl::ScopedPaint::~ScopedPaint() = default;

// static
std::unique_ptr<SkiaOutputSurface> SkiaOutputSurfaceImpl::Create(
    std::unique_ptr<SkiaOutputSurfaceDependency> deps,
    const RendererSettings& renderer_settings) {
  auto output_surface = std::make_unique<SkiaOutputSurfaceImpl>(
      util::PassKey<SkiaOutputSurfaceImpl>(), std::move(deps),
      renderer_settings);
  if (!output_surface->Initialize())
    output_surface = nullptr;
  return output_surface;
}

SkiaOutputSurfaceImpl::SkiaOutputSurfaceImpl(
    util::PassKey<SkiaOutputSurfaceImpl> /* pass_key */,
    std::unique_ptr<SkiaOutputSurfaceDependency> deps,
    const RendererSettings& renderer_settings)
    : dependency_(std::move(deps)),
      is_using_vulkan_(dependency_->IsUsingVulkan()),
      renderer_settings_(renderer_settings) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

SkiaOutputSurfaceImpl::~SkiaOutputSurfaceImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  current_paint_.reset();
  root_recorder_.reset();

  std::vector<std::unique_ptr<ImageContextImpl>> render_pass_image_contexts;
  render_pass_image_contexts.reserve(render_pass_image_cache_.size());
  for (auto& id_and_image_context : render_pass_image_cache_) {
    id_and_image_context.second->clear_image();
    render_pass_image_contexts.push_back(
        std::move(id_and_image_context.second));
  }

  base::WaitableEvent event;
  auto callback = base::BindOnce(
      [](std::vector<std::unique_ptr<ImageContextImpl>> render_passes,
         std::unique_ptr<SkiaOutputSurfaceImplOnGpu> impl_on_gpu,
         base::WaitableEvent* event) {
        if (!render_passes.empty())
          impl_on_gpu->RemoveRenderPassResource(std::move(render_passes));
        impl_on_gpu = nullptr;
        event->Signal();
      },
      std::move(render_pass_image_contexts), std::move(impl_on_gpu_), &event);
  ScheduleGpuTask(std::move(callback), std::vector<gpu::SyncToken>());
  event.Wait();

  // Delete task sequence.
  task_sequence_ = nullptr;
}

void SkiaOutputSurfaceImpl::BindToClient(OutputSurfaceClient* client) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(client);
  DCHECK(!client_);
  client_ = client;
}

void SkiaOutputSurfaceImpl::BindFramebuffer() {
  // TODO(penghuang): remove this method when GLRenderer is removed.
}

void SkiaOutputSurfaceImpl::SetDrawRectangle(const gfx::Rect& draw_rectangle) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!draw_rectangle_);
  draw_rectangle_.emplace(draw_rectangle);
}

void SkiaOutputSurfaceImpl::EnsureBackbuffer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // impl_on_gpu_ is released on the GPU thread by a posted task from
  // SkiaOutputSurfaceImpl::dtor. So it is safe to use base::Unretained.
  auto callback = base::BindOnce(&SkiaOutputSurfaceImplOnGpu::EnsureBackbuffer,
                                 base::Unretained(impl_on_gpu_.get()));
  task_sequence_->ScheduleOrRetainTask(std::move(callback),
                                       std::vector<gpu::SyncToken>());
}

void SkiaOutputSurfaceImpl::DiscardBackbuffer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // impl_on_gpu_ is released on the GPU thread by a posted task from
  // SkiaOutputSurfaceImpl::dtor. So it is safe to use base::Unretained.
  auto callback = base::BindOnce(&SkiaOutputSurfaceImplOnGpu::DiscardBackbuffer,
                                 base::Unretained(impl_on_gpu_.get()));
  task_sequence_->ScheduleOrRetainTask(std::move(callback),
                                       std::vector<gpu::SyncToken>());
}

void SkiaOutputSurfaceImpl::RecreateRootRecorder() {
  DCHECK(characterization_.isValid());
  root_recorder_.emplace(characterization_);

  // This will trigger the lazy initialization of the recorder
  ignore_result(root_recorder_->getCanvas());
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
    RecreateRootRecorder();
  } else {
    characterization = &characterization_;
    initialize_waitable_event_ = std::make_unique<base::WaitableEvent>(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
  }

  // impl_on_gpu_ is released on the GPU thread by a posted task from
  // SkiaOutputSurfaceImpl::dtor. So it is safe to use base::Unretained.
  auto callback = base::BindOnce(
      &SkiaOutputSurfaceImplOnGpu::Reshape,
      base::Unretained(impl_on_gpu_.get()), size, device_scale_factor,
      std::move(color_space), has_alpha, use_stencil, pre_transform_,
      characterization, initialize_waitable_event_.get());
  ScheduleGpuTask(std::move(callback), std::vector<gpu::SyncToken>());
}

void SkiaOutputSurfaceImpl::SetUpdateVSyncParametersCallback(
    UpdateVSyncParametersCallback callback) {
  update_vsync_parameters_callback_ = std::move(callback);
}

void SkiaOutputSurfaceImpl::SetDisplayTransformHint(
    gfx::OverlayTransform transform) {
  if (capabilities_.supports_pre_transform)
    pre_transform_ = transform;
}

gfx::OverlayTransform SkiaOutputSurfaceImpl::GetDisplayTransform() {
  return pre_transform_;
}

SkCanvas* SkiaOutputSurfaceImpl::BeginPaintCurrentFrame() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Make sure there is no unsubmitted PaintFrame or PaintRenderPass.
  DCHECK(!current_paint_);

  if (initialize_waitable_event_) {
    initialize_waitable_event_->Wait();
    initialize_waitable_event_ = nullptr;

    RecreateRootRecorder();
  }

  DCHECK(root_recorder_);

  current_paint_.emplace(&root_recorder_.value());

  if (!renderer_settings_.show_overdraw_feedback)
    return current_paint_->recorder()->getCanvas();

  DCHECK(!overdraw_surface_recorder_);
  DCHECK(renderer_settings_.show_overdraw_feedback);

  SkSurfaceCharacterization characterization = CreateSkSurfaceCharacterization(
      gfx::Size(characterization_.width(), characterization_.height()),
      BGRA_8888, false /* mipmap */, characterization_.refColorSpace());
  overdraw_surface_recorder_.emplace(characterization);
  overdraw_canvas_.emplace((overdraw_surface_recorder_->getCanvas()));

  nway_canvas_.emplace(characterization_.width(), characterization_.height());
  nway_canvas_->addCanvas(current_paint_->recorder()->getCanvas());
  nway_canvas_->addCanvas(&overdraw_canvas_.value());
  return &nway_canvas_.value();
}

void SkiaOutputSurfaceImpl::MakePromiseSkImage(ImageContext* image_context) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(current_paint_);
  DCHECK(!image_context->mailbox_holder().mailbox.IsZero());

  images_in_current_paint_.push_back(
      static_cast<ImageContextImpl*>(image_context));

  if (image_context->has_image())
    return;

  SkColorType color_type = ResourceFormatToClosestSkColorType(
      true /* gpu_compositing */, image_context->resource_format());
  GrBackendFormat backend_format = GetGrBackendFormatForTexture(
      image_context->resource_format(),
      image_context->mailbox_holder().texture_target,
      image_context->ycbcr_info());
  image_context->SetImage(
      current_paint_->recorder()->makePromiseTexture(
          backend_format, image_context->size().width(),
          image_context->size().height(), GrMipMapped::kNo,
          image_context->origin(), color_type, image_context->alpha_type(),
          image_context->color_space(), Fulfill /* fulfillProc */,
          DoNothing /* releaseProc */, DoNothing /* doneProc */,
          image_context /* context */),
      backend_format);

  if (image_context->mailbox_holder().sync_token.HasData()) {
    resource_sync_tokens_.push_back(image_context->mailbox_holder().sync_token);
    image_context->mutable_mailbox_holder()->sync_token.Clear();
  }
}

sk_sp<SkImage> SkiaOutputSurfaceImpl::MakePromiseSkImageFromYUV(
    const std::vector<ImageContext*>& contexts,
    SkYUVColorSpace yuv_color_space,
    sk_sp<SkColorSpace> dst_color_space,
    bool has_alpha) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(current_paint_);
  DCHECK((has_alpha && (contexts.size() == 3 || contexts.size() == 4)) ||
         (!has_alpha && (contexts.size() == 2 || contexts.size() == 3)));

  SkYUVAIndex indices[4];
  PrepareYUVATextureIndices(contexts, has_alpha, indices);

  GrBackendFormat formats[4] = {};
  SkISize yuva_sizes[4] = {};
  SkDeferredDisplayListRecorder::PromiseImageTextureContext
      texture_contexts[4] = {};
  for (size_t i = 0; i < contexts.size(); ++i) {
    auto* context = static_cast<ImageContextImpl*>(contexts[i]);
    DCHECK(context->origin() == kTopLeft_GrSurfaceOrigin);
    formats[i] = GetGrBackendFormatForTexture(
        context->resource_format(), context->mailbox_holder().texture_target);
    yuva_sizes[i].set(context->size().width(), context->size().height());

    // NOTE: We don't have promises for individual planes, but still need format
    // for fallback
    context->SetImage(nullptr, formats[i]);

    if (context->mailbox_holder().sync_token.HasData()) {
      resource_sync_tokens_.push_back(context->mailbox_holder().sync_token);
      context->mutable_mailbox_holder()->sync_token.Clear();
    }
    images_in_current_paint_.push_back(context);
    texture_contexts[i] = context;
  }

  auto image = current_paint_->recorder()->makeYUVAPromiseTexture(
      yuv_color_space, formats, yuva_sizes, indices, yuva_sizes[0].width(),
      yuva_sizes[0].height(), kTopLeft_GrSurfaceOrigin, dst_color_space,
      Fulfill, DoNothing, DoNothing, texture_contexts);
  DCHECK(image);
  return image;
}

void SkiaOutputSurfaceImpl::ReleaseImageContexts(
    std::vector<std::unique_ptr<ImageContext>> image_contexts) {
  if (image_contexts.empty())
    return;

  // impl_on_gpu_ is released on the GPU thread by a posted task from
  // SkiaOutputSurfaceImpl::dtor. So it is safe to use base::Unretained.
  auto callback = base::BindOnce(
      &SkiaOutputSurfaceImplOnGpu::ReleaseImageContexts,
      base::Unretained(impl_on_gpu_.get()), std::move(image_contexts));
  task_sequence_->ScheduleOrRetainTask(std::move(callback),
                                       std::vector<gpu::SyncToken>());
}

std::unique_ptr<ExternalUseClient::ImageContext>
SkiaOutputSurfaceImpl::CreateImageContext(const gpu::MailboxHolder& holder,
                                          const gfx::Size& size,
                                          ResourceFormat format,
                                          sk_sp<SkColorSpace> color_space) {
  return std::make_unique<ImageContextImpl>(holder, size, format,
                                            std::move(color_space));
}

void SkiaOutputSurfaceImpl::SkiaSwapBuffers(OutputSurfaceFrame frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!current_paint_);
  // impl_on_gpu_ is released on the GPU thread by a posted task from
  // SkiaOutputSurfaceImpl::dtor. So it is safe to use base::Unretained.
  auto callback =
      base::BindOnce(&SkiaOutputSurfaceImplOnGpu::SwapBuffers,
                     base::Unretained(impl_on_gpu_.get()), std::move(frame),
                     std::move(deferred_framebuffer_draw_closure_));
  ScheduleGpuTask(std::move(callback), std::move(resource_sync_tokens_));

  // Recreate |root_recorder_| after SwapBuffers has been scheduled on GPU
  // thread to save some time in BeginPaintCurrentFrame
  // TODO(vasilyt): reuse root recorder
  RecreateRootRecorder();
}

void SkiaOutputSurfaceImpl::ScheduleOutputSurfaceAsOverlay(
    OverlayProcessor::OutputSurfaceOverlayPlane output_surface_plane) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // impl_on_gpu_ is released on the GPU thread by a posted task from
  // SkiaOutputSurfaceImpl::dtor. So it is safe to use base::Unretained.
  auto callback = base::BindOnce(
      &SkiaOutputSurfaceImplOnGpu::ScheduleOutputSurfaceAsOverlay,
      base::Unretained(impl_on_gpu_.get()), std::move(output_surface_plane));
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
  DCHECK(!current_paint_);
  DCHECK(resource_sync_tokens_.empty());

  SkSurfaceCharacterization c = CreateSkSurfaceCharacterization(
      surface_size, format, mipmap, std::move(color_space));
  current_paint_.emplace(c, id);
  return current_paint_->recorder()->getCanvas();
}

gpu::SyncToken SkiaOutputSurfaceImpl::SubmitPaint(
    base::OnceClosure on_finished) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(current_paint_);
  DCHECK(!deferred_framebuffer_draw_closure_);
  // If current_render_pass_id_ is not 0, we are painting a render pass.
  // Otherwise we are painting a frame.

  bool painting_render_pass = current_paint_->render_pass_id() != 0;

  gpu::SyncToken sync_token(
      gpu::CommandBufferNamespace::VIZ_SKIA_OUTPUT_SURFACE,
      impl_on_gpu_->command_buffer_id(), ++sync_fence_release_);
  sync_token.SetVerifyFlush();

  auto ddl = current_paint_->recorder()->detach();
  DCHECK(ddl);
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
  if (painting_render_pass) {
    auto it = render_pass_image_cache_.find(current_paint_->render_pass_id());
    if (it != render_pass_image_cache_.end()) {
      // We are going to overwrite the render pass, so we need reset the
      // image_context, so a new promise image will be created when the
      // MakePromiseSkImageFromRenderPass() is called.
      it->second->clear_image();
    }
    DCHECK(!on_finished);
    auto closure = base::BindOnce(
        &SkiaOutputSurfaceImplOnGpu::FinishPaintRenderPass,
        base::Unretained(impl_on_gpu_.get()), current_paint_->render_pass_id(),
        std::move(ddl), std::move(images_in_current_paint_),
        resource_sync_tokens_, sync_fence_release_);
    ScheduleGpuTask(std::move(closure), std::move(resource_sync_tokens_));
  } else {
    deferred_framebuffer_draw_closure_ = base::BindOnce(
        &SkiaOutputSurfaceImplOnGpu::FinishPaintCurrentFrame,
        base::Unretained(impl_on_gpu_.get()), std::move(ddl),
        std::move(overdraw_ddl), std::move(images_in_current_paint_),
        resource_sync_tokens_, sync_fence_release_, std::move(on_finished),
        draw_rectangle_);
    draw_rectangle_.reset();
  }
  images_in_current_paint_.clear();
  current_paint_.reset();
  return sync_token;
}

sk_sp<SkImage> SkiaOutputSurfaceImpl::MakePromiseSkImageFromRenderPass(
    const RenderPassId& id,
    const gfx::Size& size,
    ResourceFormat format,
    bool mipmap,
    sk_sp<SkColorSpace> color_space) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(current_paint_);

  auto& image_context = render_pass_image_cache_[id];
  if (!image_context) {
    image_context = std::make_unique<ImageContextImpl>(id, size, format, mipmap,
                                                       std::move(color_space));
  }
  if (!image_context->has_image()) {
    SkColorType color_type =
        ResourceFormatToClosestSkColorType(true /* gpu_compositing */, format);
    GrBackendFormat backend_format =
        GetGrBackendFormatForTexture(format, GL_TEXTURE_2D);
    image_context->SetImage(
        current_paint_->recorder()->makePromiseTexture(
            backend_format, image_context->size().width(),
            image_context->size().height(), image_context->mipmap(),
            image_context->origin(), color_type, image_context->alpha_type(),
            image_context->color_space(), Fulfill, DoNothing, DoNothing,
            image_context.get()),
        backend_format);
    DCHECK(image_context->has_image());
  }
  images_in_current_paint_.push_back(image_context.get());
  return image_context->image();
}

void SkiaOutputSurfaceImpl::RemoveRenderPassResource(
    std::vector<RenderPassId> ids) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!ids.empty());

  std::vector<std::unique_ptr<ImageContextImpl>> image_contexts;
  image_contexts.reserve(ids.size());
  for (const auto id : ids) {
    auto it = render_pass_image_cache_.find(id);
    // If the render pass was only used for a copy request, there won't be a
    // matching entry in |render_pass_image_cache_|.
    if (it != render_pass_image_cache_.end()) {
      it->second->clear_image();
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
                                 geometry, color_space, std::move(request),
                                 std::move(deferred_framebuffer_draw_closure_));
  ScheduleGpuTask(std::move(callback), std::move(resource_sync_tokens_));
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

bool SkiaOutputSurfaceImpl::Initialize() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Before start scheduling GPU task, set up |task_sequence_|.
  task_sequence_ = dependency_->CreateSequence();

  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  bool result = false;
  auto callback = base::BindOnce(&SkiaOutputSurfaceImpl::InitializeOnGpuThread,
                                 base::Unretained(this), &event, &result);
  ScheduleGpuTask(std::move(callback), std::vector<gpu::SyncToken>());
  event.Wait();
  return result;
}

void SkiaOutputSurfaceImpl::InitializeOnGpuThread(base::WaitableEvent* event,
                                                  bool* result) {
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
  impl_on_gpu_ = SkiaOutputSurfaceImplOnGpu::Create(
      dependency_.get(), renderer_settings_, task_sequence_->GetSequenceId(),
      did_swap_buffer_complete_callback, buffer_presented_callback,
      context_lost_callback);
  if (!impl_on_gpu_) {
    *result = false;
  } else {
    capabilities_ = impl_on_gpu_->capabilities();
    is_displayed_as_overlay_ = impl_on_gpu_->IsDisplayedAsOverlay();
    *result = true;
  }
}

SkSurfaceCharacterization
SkiaOutputSurfaceImpl::CreateSkSurfaceCharacterization(
    const gfx::Size& surface_size,
    ResourceFormat format,
    bool mipmap,
    sk_sp<SkColorSpace> color_space) {
  auto gr_context_thread_safe = impl_on_gpu_->GetGrContextThreadSafeProxy();
  auto cache_max_resource_bytes = impl_on_gpu_->max_resource_cache_bytes();
  // LegacyFontHost will get LCD text and skia figures out what type to use.
  SkSurfaceProps surface_props(0 /*flags */,
                               SkSurfaceProps::kLegacyFontHost_InitType);
  auto color_type =
      ResourceFormatToClosestSkColorType(true /* gpu_compositing */, format);
  auto image_info =
      SkImageInfo::Make(surface_size.width(), surface_size.height(), color_type,
                        kPremul_SkAlphaType, std::move(color_space));
  auto backend_format = gr_context_thread_safe->defaultBackendFormat(
      color_type, GrRenderable::kYes);
  DCHECK(backend_format.isValid());
  auto characterization = gr_context_thread_safe->createCharacterization(
      cache_max_resource_bytes, image_info, backend_format, 0 /* sampleCount */,
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
  client_->DidReceiveSwapBuffersAck(params.swap_response.timings);
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

void SkiaOutputSurfaceImpl::ScheduleGpuTaskForTesting(
    base::OnceClosure callback,
    std::vector<gpu::SyncToken> sync_tokens) {
  ScheduleGpuTask(std::move(callback), std::move(sync_tokens));
}

void SkiaOutputSurfaceImpl::ScheduleGpuTask(
    base::OnceClosure callback,
    std::vector<gpu::SyncToken> sync_tokens) {
  task_sequence_->ScheduleTask(std::move(callback), std::move(sync_tokens));
}

GrBackendFormat SkiaOutputSurfaceImpl::GetGrBackendFormatForTexture(
    ResourceFormat resource_format,
    uint32_t gl_texture_target,
    base::Optional<gpu::VulkanYCbCrInfo> ycbcr_info) {
  if (!is_using_vulkan_) {
    DCHECK(!ycbcr_info);
    // Convert internal format from GLES2 to platform GL.
    unsigned int texture_storage_format = gpu::GetGrGLBackendTextureFormat(
        impl_on_gpu_->GetFeatureInfo(), resource_format);

    return GrBackendFormat::MakeGL(texture_storage_format, gl_texture_target);
  } else {
#if BUILDFLAG(ENABLE_VULKAN)
    if (!ycbcr_info)
      return GrBackendFormat::MakeVk(ToVkFormat(resource_format));

    VkFormat format = ycbcr_info->external_format ? VK_FORMAT_UNDEFINED
                                                  : ToVkFormat(resource_format);
    GrVkYcbcrConversionInfo gr_ycbcr_info(
        format, ycbcr_info->external_format,
        static_cast<VkSamplerYcbcrModelConversion>(
            ycbcr_info->suggested_ycbcr_model),
        static_cast<VkSamplerYcbcrRange>(ycbcr_info->suggested_ycbcr_range),
        static_cast<VkChromaLocation>(ycbcr_info->suggested_xchroma_offset),
        static_cast<VkChromaLocation>(ycbcr_info->suggested_ychroma_offset),
        VK_FILTER_LINEAR,  // VkFilter
        0,                 // VkBool32 forceExplicitReconstruction,
        static_cast<VkFormatFeatureFlags>(ycbcr_info->format_features));
    return GrBackendFormat::MakeVk(gr_ycbcr_info);
#else
    NOTREACHED();
    return GrBackendFormat();
#endif
  }
}

void SkiaOutputSurfaceImpl::SwapBuffers(OutputSurfaceFrame frame) {
  NOTREACHED();
}

uint32_t SkiaOutputSurfaceImpl::GetFramebufferCopyTextureFormat() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  return GL_RGB;
}

bool SkiaOutputSurfaceImpl::IsDisplayedAsOverlayPlane() const {
  return is_displayed_as_overlay_;
}

unsigned SkiaOutputSurfaceImpl::GetOverlayTextureId() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return 0;
}

gfx::BufferFormat SkiaOutputSurfaceImpl::GetOverlayBufferFormat() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return gfx::BufferFormat::RGBX_8888;
}

bool SkiaOutputSurfaceImpl::HasExternalStencilTest() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  return false;
}

void SkiaOutputSurfaceImpl::ApplyExternalStencil() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

unsigned SkiaOutputSurfaceImpl::UpdateGpuFence() {
  return 0;
}

void SkiaOutputSurfaceImpl::SetNeedsSwapSizeNotifications(
    bool needs_swap_size_notifications) {
  needs_swap_size_notifications_ = needs_swap_size_notifications;
}

void SkiaOutputSurfaceImpl::AddContextLostObserver(
    ContextLostObserver* observer) {
  observers_.AddObserver(observer);
}

void SkiaOutputSurfaceImpl::RemoveContextLostObserver(
    ContextLostObserver* observer) {
  observers_.RemoveObserver(observer);
}

void SkiaOutputSurfaceImpl::PrepareYUVATextureIndices(
    const std::vector<ImageContext*>& contexts,
    bool has_alpha,
    SkYUVAIndex indices[4]) {
  DCHECK((has_alpha && (contexts.size() == 3 || contexts.size() == 4)) ||
         (!has_alpha && (contexts.size() == 2 || contexts.size() == 3)));

  bool uv_interleaved = has_alpha ? contexts.size() == 3 : contexts.size() == 2;

  indices[SkYUVAIndex::kY_Index].fIndex = 0;
  indices[SkYUVAIndex::kY_Index].fChannel = SkColorChannel::kR;

  if (uv_interleaved) {
    indices[SkYUVAIndex::kU_Index].fIndex = 1;
    indices[SkYUVAIndex::kU_Index].fChannel = SkColorChannel::kR;

    indices[SkYUVAIndex::kV_Index].fIndex = 1;
    indices[SkYUVAIndex::kV_Index].fChannel = SkColorChannel::kG;

    indices[SkYUVAIndex::kA_Index].fIndex = has_alpha ? 2 : -1;
    indices[SkYUVAIndex::kA_Index].fChannel = SkColorChannel::kR;
  } else {
    indices[SkYUVAIndex::kU_Index].fIndex = 1;
    indices[SkYUVAIndex::kU_Index].fChannel = SkColorChannel::kR;

    indices[SkYUVAIndex::kV_Index].fIndex = 2;
    indices[SkYUVAIndex::kV_Index].fChannel = SkColorChannel::kR;

    indices[SkYUVAIndex::kA_Index].fIndex = has_alpha ? 3 : -1;
    indices[SkYUVAIndex::kA_Index].fChannel = SkColorChannel::kR;
  }
}

void SkiaOutputSurfaceImpl::ContextLost() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (auto& observer : observers_)
    observer.OnContextLost();
}

}  // namespace viz
