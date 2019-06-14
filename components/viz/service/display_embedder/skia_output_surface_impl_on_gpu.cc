// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_surface_impl_on_gpu.h"

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/optional.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_util.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/skia_helper.h"
#include "components/viz/service/display/gl_renderer_copier.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/texture_deleter.h"
#include "components/viz/service/display_embedder/direct_context_provider.h"
#include "components/viz/service/display_embedder/image_context.h"
#include "components/viz/service/display_embedder/skia_output_device.h"
#include "components/viz/service/display_embedder/skia_output_device_gl.h"
#include "components/viz/service/display_embedder/skia_output_device_offscreen.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/command_buffer/service/context_state.h"
#include "gpu/command_buffer/service/gr_shader_cache.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/texture_base.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/ipc/common/gpu_client_ids.h"
#include "gpu/ipc/common/gpu_surface_lookup.h"
#include "gpu/ipc/service/image_transport_surface.h"
#include "gpu/vulkan/buildflags.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/private/SkDeferredDisplayList.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/skia_util.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/init/gl_factory.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "components/viz/service/display_embedder/skia_output_device_vulkan.h"
#endif

#if BUILDFLAG(ENABLE_VULKAN) && defined(USE_X11)
#include "components/viz/service/display_embedder/skia_output_device_x11.h"
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/platform_window_surface.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#endif

namespace viz {

namespace {

struct ReadPixelsContext {
  ReadPixelsContext(std::unique_ptr<CopyOutputRequest> request,
                    const gfx::Rect& result_rect)
      : request(std::move(request)), result_rect(result_rect) {}

  std::unique_ptr<CopyOutputRequest> request;
  gfx::Rect result_rect;
};

class CopyOutputResultYUV : public CopyOutputResult {
 public:
  CopyOutputResultYUV(const gfx::Rect& rect,
                      const void* data[3],
                      size_t row_bytes[3])
      : CopyOutputResult(Format::I420_PLANES, rect) {
    DCHECK_EQ(0, size().width() % 2);
    DCHECK_EQ(0, size().height() % 2);
    for (int i = 0; i < 3; ++i) {
      data_[i] = std::make_unique<uint8_t[]>(row_bytes[i] * height(i));
      memcpy(data_[i].get(), data[i], row_bytes[i] * height(i));
      row_bytes_[i] = row_bytes[i];
    }
  }

  // CopyOutputResult implementation.
  bool ReadI420Planes(uint8_t* y_out,
                      int y_out_stride,
                      uint8_t* u_out,
                      int u_out_stride,
                      uint8_t* v_out,
                      int v_out_stride) const override {
    const auto CopyPlane = [](const uint8_t* src, int src_stride, int width,
                              int height, uint8_t* out, int out_stride) {
      for (int i = 0; i < height; ++i, src += src_stride, out += out_stride) {
        memcpy(out, src, width);
      }
    };
    CopyPlane(data_[0].get(), row_bytes_[0], width(0), height(0), y_out,
              y_out_stride);
    CopyPlane(data_[1].get(), row_bytes_[1], width(1), height(1), u_out,
              u_out_stride);
    CopyPlane(data_[2].get(), row_bytes_[2], width(2), height(2), v_out,
              v_out_stride);
    return true;
  }

 private:
  uint32_t width(int plane) const {
    if (plane == 0)
      return size().width();
    else
      return size().width() / 2;
  }

  uint32_t height(int plane) const {
    if (plane == 0)
      return size().height();
    else
      return size().height() / 2;
  }

  std::unique_ptr<uint8_t[]> data_[3];
  size_t row_bytes_[3];
};

void OnYUVReadbackDone(void* c, const void* data[3], size_t row_bytes[3]) {
  std::unique_ptr<ReadPixelsContext> context(
      static_cast<ReadPixelsContext*>(c));
  if (!data) {
    // This will automatically send an empty result.
    return;
  }
  std::unique_ptr<CopyOutputResult> result =
      std::make_unique<CopyOutputResultYUV>(context->result_rect, data,
                                            row_bytes);
  context->request->SendResult(std::move(result));
}

}  // namespace

class SkiaOutputSurfaceImplOnGpu::ScopedPromiseImageAccess {
 public:
  ScopedPromiseImageAccess(SkiaOutputSurfaceImplOnGpu* impl_on_gpu,
                           std::vector<ImageContext*> image_contexts)
      : impl_on_gpu_(impl_on_gpu), image_contexts_(std::move(image_contexts)) {
    begin_semaphores_.reserve(image_contexts_.size());
    // We may need one more space for the swap buffer semaphore.
    end_semaphores_.reserve(image_contexts_.size() + 1);
    // TODO(penghuang): gather begin read access semaphores from shared images.
    // https://crbug.com/944194
    impl_on_gpu_->BeginAccessImages(image_contexts_, &begin_semaphores_,
                                    &end_semaphores_);
  }

  ~ScopedPromiseImageAccess() {
    // TODO(penghuang): end shared image access with meaningful semaphores.
    // https://crbug.com/944194
    impl_on_gpu_->EndAccessImages(image_contexts_);
  }

  std::vector<GrBackendSemaphore>& begin_semaphores() {
    return begin_semaphores_;
  }

  std::vector<GrBackendSemaphore>& end_semaphores() { return end_semaphores_; }

 private:
  SkiaOutputSurfaceImplOnGpu* const impl_on_gpu_;
  std::vector<ImageContext*> image_contexts_;
  std::vector<GrBackendSemaphore> begin_semaphores_;
  std::vector<GrBackendSemaphore> end_semaphores_;

  DISALLOW_COPY_AND_ASSIGN(ScopedPromiseImageAccess);
};

// Skia gr_context() and |context_provider_| share an underlying GLContext.
// Each of them caches some GL state. Interleaving usage could make cached
// state inconsistent with GL state. Using a ScopedUseContextProvider whenever
// |context_provider_| could be accessed (e.g. processing completed queries),
// will keep cached state consistent with driver GL state.
class SkiaOutputSurfaceImplOnGpu::ScopedUseContextProvider {
 public:
  ScopedUseContextProvider(SkiaOutputSurfaceImplOnGpu* impl_on_gpu,
                           GLuint texture_client_id)
      : impl_on_gpu_(impl_on_gpu) {
    if (!impl_on_gpu_->MakeCurrent(true /* need_fbo0 */)) {
      valid_ = false;
      return;
    }

    // GLRendererCopier uses context_provider_->ContextGL(), which caches GL
    // state and removes state setting calls that it considers redundant. To get
    // to a known GL state, we first set driver GL state and then make client
    // side consistent with that.
    auto* api = impl_on_gpu_->api_;
    api->glBindFramebufferEXTFn(GL_FRAMEBUFFER, 0);
    impl_on_gpu_->context_provider_->SetGLRendererCopierRequiredState(
        texture_client_id);
  }

  ~ScopedUseContextProvider() {
    if (valid_)
      impl_on_gpu_->gr_context()->resetContext();
  }

  bool valid() { return valid_; }

 private:
  SkiaOutputSurfaceImplOnGpu* const impl_on_gpu_;
  bool valid_ = true;

  DISALLOW_COPY_AND_ASSIGN(ScopedUseContextProvider);
};

namespace {

base::AtomicSequenceNumber g_next_command_buffer_id;

scoped_refptr<gpu::gles2::FeatureInfo> CreateFeatureInfo(
    SkiaOutputSurfaceDependency* deps) {
  return base::MakeRefCounted<gpu::gles2::FeatureInfo>(
      deps->GetGpuDriverBugWorkarounds(), deps->GetGpuFeatureInfo());
}

scoped_refptr<gpu::SyncPointClientState> CreateSyncPointClientState(
    SkiaOutputSurfaceDependency* deps) {
  auto command_buffer_id = gpu::CommandBufferId::FromUnsafeValue(
      g_next_command_buffer_id.GetNext() + 1);
  return deps->GetSyncPointManager()->CreateSyncPointClientState(
      gpu::CommandBufferNamespace::VIZ_SKIA_OUTPUT_SURFACE, command_buffer_id,
      deps->GetSequenceId());
}

std::unique_ptr<gpu::SharedImageRepresentationFactory>
CreateSharedImageRepresentationFactory(SkiaOutputSurfaceDependency* deps) {
  // TODO(https://crbug.com/899905): Use a real MemoryTracker, not nullptr.
  return std::make_unique<gpu::SharedImageRepresentationFactory>(
      deps->GetSharedImageManager(), nullptr);
}

class ScopedSurfaceToTexture {
 public:
  ScopedSurfaceToTexture(scoped_refptr<DirectContextProvider> context_provider,
                         SkSurface* surface)
      : context_provider_(context_provider) {
    GrBackendTexture skia_texture =
        surface->getBackendTexture(SkSurface::kFlushRead_BackendHandleAccess);
    GrGLTextureInfo gl_texture_info;
    skia_texture.getGLTextureInfo(&gl_texture_info);
    GLuint client_id = context_provider_->GenClientTextureId();
    auto* texture_manager = context_provider_->texture_manager();
    texture_ref_ =
        texture_manager->CreateTexture(client_id, gl_texture_info.fID);
    texture_manager->SetTarget(texture_ref_.get(), gl_texture_info.fTarget);
    texture_manager->SetLevelInfo(
        texture_ref_.get(), gl_texture_info.fTarget,
        /*level=*/0,
        /*internal_format=*/GL_RGBA, surface->width(), surface->height(),
        /*depth=*/1, /*border=*/0,
        /*format=*/GL_RGBA, /*type=*/GL_UNSIGNED_BYTE,
        /*cleared_rect=*/gfx::Rect(surface->width(), surface->height()));
  }

  ~ScopedSurfaceToTexture() {
    context_provider_->DeleteClientTextureId(client_id());

    // Skia owns the texture. It will delete it when it is done.
    texture_ref_->ForceContextLost();
  }

  GLuint client_id() { return texture_ref_->client_id(); }

 private:
  scoped_refptr<DirectContextProvider> context_provider_;
  scoped_refptr<gpu::gles2::TextureRef> texture_ref_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSurfaceToTexture);
};

// This SingleThreadTaskRunner runs tasks on the GPU main thread, where
// DirectContextProvider can safely service calls. It wraps all posted tasks to
// ensure that |impl_on_gpu_->context_provider_| is made current and in a known
// state when the task is run. If |impl_on_gpu| is destructed, pending tasks are
// no-oped when they are run.
class ContextCurrentTaskRunner : public base::SingleThreadTaskRunner {
 public:
  explicit ContextCurrentTaskRunner(SkiaOutputSurfaceImplOnGpu* impl_on_gpu)
      : real_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        impl_on_gpu_(impl_on_gpu) {}

  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    return real_task_runner_->PostDelayedTask(
        from_here, WrapClosure(std::move(task)), delay);
  }

  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override {
    return real_task_runner_->PostNonNestableDelayedTask(
        from_here, WrapClosure(std::move(task)), delay);
  }

  bool RunsTasksInCurrentSequence() const override {
    return real_task_runner_->RunsTasksInCurrentSequence();
  }

 private:
  base::OnceClosure WrapClosure(base::OnceClosure task) {
    return base::BindOnce(
        [](base::WeakPtr<SkiaOutputSurfaceImplOnGpu> impl_on_gpu,
           base::OnceClosure task) {
          if (!impl_on_gpu)
            return;
          SkiaOutputSurfaceImplOnGpu::ScopedUseContextProvider scoped_use(
              impl_on_gpu.get(), /*texture_client_id=*/0);
          if (!scoped_use.valid())
            return;

          std::move(task).Run();
        },
        impl_on_gpu_->weak_ptr(), std::move(task));
  }

  ~ContextCurrentTaskRunner() override = default;

  scoped_refptr<base::SingleThreadTaskRunner> real_task_runner_;
  SkiaOutputSurfaceImplOnGpu* const impl_on_gpu_;

  DISALLOW_COPY_AND_ASSIGN(ContextCurrentTaskRunner);
};

class DirectContextProviderDelegateImpl : public DirectContextProviderDelegate,
                                          public gpu::SharedImageInterface {
 public:
  DirectContextProviderDelegateImpl(
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuDriverBugWorkarounds& workarounds,
      const gpu::GpuFeatureInfo& gpu_feature_info,
      gpu::SharedContextState* context_state,
      gpu::MailboxManager* mailbox_manager,
      gpu::SharedImageManager* shared_image_manager,
      scoped_refptr<gpu::SyncPointClientState> sync_point_client_state)
      : shared_image_manager_(shared_image_manager),
        shared_image_factory_(gpu_preferences,
                              workarounds,
                              gpu_feature_info,
                              context_state,
                              mailbox_manager,
                              shared_image_manager,
                              nullptr /* image_factory */,
                              nullptr /* memory_tracker */,
                              true /* is_using_skia_renderer */),
        sync_point_client_state_(sync_point_client_state) {}

  ~DirectContextProviderDelegateImpl() override {
    sync_point_client_state_->Destroy();
  }

  // SharedImageInterface implementation:
  gpu::Mailbox CreateSharedImage(ResourceFormat format,
                                 const gfx::Size& size,
                                 const gfx::ColorSpace& color_space,
                                 uint32_t usage) override {
    auto mailbox = gpu::Mailbox::GenerateForSharedImage();
    if (shared_image_factory_.CreateSharedImage(mailbox, format, size,
                                                color_space, usage))
      return mailbox;
    return gpu::Mailbox();
  }

  gpu::Mailbox CreateSharedImage(
      ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      uint32_t usage,
      base::span<const uint8_t> pixel_data) override {
    auto mailbox = gpu::Mailbox::GenerateForSharedImage();
    if (shared_image_factory_.CreateSharedImage(mailbox, format, size,
                                                color_space, usage, pixel_data))
      return mailbox;
    return gpu::Mailbox();
  }

  gpu::Mailbox CreateSharedImage(
      gfx::GpuMemoryBuffer* gpu_memory_buffer,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      const gfx::ColorSpace& color_space,
      uint32_t usage) override {
    // We do not support creating GMB backed SharedImages.
    NOTIMPLEMENTED();
    return gpu::Mailbox();
  }
  void UpdateSharedImage(const gpu::SyncToken& sync_token,
                         std::unique_ptr<gfx::GpuFence> acquire_fence,
                         const gpu::Mailbox& mailbox) override {
    NOTREACHED();
  }

  void UpdateSharedImage(const gpu::SyncToken& sync_token,
                         const gpu::Mailbox& mailbox) override {
    DCHECK(!ShouldWait(sync_token))
        << "Cannot UpdateSharedImage with SyncToken from different "
           "command buffer.";
    shared_image_factory_.UpdateSharedImage(mailbox);
  }

  void DestroySharedImage(const gpu::SyncToken& sync_token,
                          const gpu::Mailbox& mailbox) override {
    DCHECK(!ShouldWait(sync_token))
        << "Cannot DestroySharedImage with SyncToken from different "
           "command buffer.";
    shared_image_factory_.DestroySharedImage(mailbox);
  }

#if defined(OS_WIN)
  SwapChainMailboxes CreateSwapChain(ResourceFormat format,
                                     const gfx::Size& size,
                                     const gfx::ColorSpace& color_space,
                                     uint32_t usage) override {
    NOTREACHED();
    return {};
  }

  void PresentSwapChain(const gpu::SyncToken& sync_token,
                        const gpu::Mailbox& mailbox) override {
    NOTREACHED();
  }
#endif  // OS_WIN

  gpu::SyncToken GenUnverifiedSyncToken() override {
    return gpu::SyncToken(sync_point_client_state_->namespace_id(),
                          sync_point_client_state_->command_buffer_id(),
                          GenerateFenceSyncRelease());
  }

  gpu::SyncToken GenVerifiedSyncToken() override {
    gpu::SyncToken sync_token = GenUnverifiedSyncToken();
    sync_token.SetVerifyFlush();
    return sync_token;
  }

  // DirectContextProviderDelegate implementation.
  gpu::SharedImageManager* GetSharedImageManager() override {
    return shared_image_manager_;
  }

  gpu::SharedImageInterface* GetSharedImageInterface() override { return this; }

  gpu::CommandBufferNamespace GetNamespaceID() const override {
    return sync_point_client_state_->namespace_id();
  }

  gpu::CommandBufferId GetCommandBufferID() const override {
    return sync_point_client_state_->command_buffer_id();
  }

  uint64_t GenerateFenceSyncRelease() override {
    uint64_t release = ++sync_fence_release_;
    // Release fence immediately because the relevant GPU calls were already
    // issued.
    sync_point_client_state_->ReleaseFenceSync(release);
    return release;
  }

  void SignalSyncToken(const gpu::SyncToken& sync_token,
                       base::OnceClosure callback) override {
    base::RepeatingClosure maybe_pass_callback =
        base::AdaptCallbackForRepeating(std::move(callback));
    if (!sync_point_client_state_->Wait(sync_token, maybe_pass_callback)) {
      maybe_pass_callback.Run();
    }
  }

 private:
  bool ShouldWait(const gpu::SyncToken& sync_token) {
    // Don't wait on an invalid SyncToken.
    if (!sync_token.HasData())
      return false;

    // Don't wait on SyncTokens our own sync tokens because we've already issued
    // the relevant calls to the GPU.
    return sync_point_client_state_->namespace_id() !=
               sync_token.namespace_id() ||
           sync_point_client_state_->command_buffer_id() !=
               sync_token.command_buffer_id();
  }

  gpu::SharedImageManager* const shared_image_manager_;
  gpu::SharedImageFactory shared_image_factory_;
  scoped_refptr<gpu::SyncPointClientState> sync_point_client_state_;
  uint64_t sync_fence_release_ = 0;

  DISALLOW_COPY_AND_ASSIGN(DirectContextProviderDelegateImpl);
};

}  // namespace

SkiaOutputSurfaceImplOnGpu::OffscreenSurface::OffscreenSurface() = default;

SkiaOutputSurfaceImplOnGpu::OffscreenSurface::~OffscreenSurface() = default;

SkiaOutputSurfaceImplOnGpu::OffscreenSurface::OffscreenSurface(
    OffscreenSurface&& offscreen_surface) = default;

SkiaOutputSurfaceImplOnGpu::OffscreenSurface&
SkiaOutputSurfaceImplOnGpu::OffscreenSurface::operator=(
    OffscreenSurface&& offscreen_surface) = default;

SkSurface* SkiaOutputSurfaceImplOnGpu::OffscreenSurface::surface() const {
  return surface_.get();
}

sk_sp<SkPromiseImageTexture>
SkiaOutputSurfaceImplOnGpu::OffscreenSurface::fulfill() {
  DCHECK(surface_);
  if (!promise_texture_) {
    promise_texture_ = SkPromiseImageTexture::Make(
        surface_->getBackendTexture(SkSurface::kFlushRead_BackendHandleAccess));
  }
  return promise_texture_;
}

void SkiaOutputSurfaceImplOnGpu::OffscreenSurface::set_surface(
    sk_sp<SkSurface> surface) {
  surface_ = std::move(surface);
  promise_texture_ = {};
}

SkiaOutputSurfaceImplOnGpu::SkiaOutputSurfaceImplOnGpu(
    SkiaOutputSurfaceDependency* deps,
    const RendererSettings& renderer_settings,
    const DidSwapBufferCompleteCallback& did_swap_buffer_complete_callback,
    const BufferPresentedCallback& buffer_presented_callback,
    const ContextLostCallback& context_lost_callback)
    : dependency_(std::move(deps)),
      feature_info_(CreateFeatureInfo(dependency_)),
      sync_point_client_state_(CreateSyncPointClientState(dependency_)),
      shared_image_representation_factory_(
          CreateSharedImageRepresentationFactory(dependency_)),
      vulkan_context_provider_(dependency_->GetVulkanContextProvider()),
      renderer_settings_(renderer_settings),
      did_swap_buffer_complete_callback_(did_swap_buffer_complete_callback),
      buffer_presented_callback_(buffer_presented_callback),
      context_lost_callback_(context_lost_callback),
      weak_ptr_factory_(this) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();
#if defined(USE_OZONE)
  window_surface_ =
      ui::OzonePlatform::GetInstance()
          ->GetSurfaceFactoryOzone()
          ->CreatePlatformWindowSurface(dependency_->GetSurfaceHandle());
#endif
  gpu_preferences_ = dependency_->GetGpuPreferences();

  if (is_using_vulkan())
    InitializeForVulkan();
  else
    InitializeForGL();
}

SkiaOutputSurfaceImplOnGpu::~SkiaOutputSurfaceImplOnGpu() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // |context_provider_| and clients want either the context to be lost or made
  // current on destruction.
  if (MakeCurrent(false /* need_fbo0 */)) {
    // This ensures any outstanding callbacks for promise images are performed.
    gr_context()->flush();
  }
  copier_ = nullptr;
  texture_deleter_ = nullptr;
  context_provider_ = nullptr;

  sync_point_client_state_->Destroy();
}

void SkiaOutputSurfaceImplOnGpu::Reshape(
    const gfx::Size& size,
    float device_scale_factor,
    const gfx::ColorSpace& color_space,
    bool has_alpha,
    bool use_stencil,
    SkSurfaceCharacterization* characterization,
    base::WaitableEvent* event) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(gr_context());

  base::ScopedClosureRunner scoped_runner;
  if (event) {
    scoped_runner.ReplaceClosure(
        base::BindOnce(&base::WaitableEvent::Signal, base::Unretained(event)));
  }

  if (!MakeCurrent(!dependency_->IsOffscreen() /* need_fbo0 */))
    return;

  size_ = size;
  color_space_ = color_space;
  output_device_->Reshape(size_, device_scale_factor, color_space, has_alpha);

  if (characterization) {
    output_sk_surface()->characterize(characterization);
    DCHECK(characterization->isValid());
  }
}

void SkiaOutputSurfaceImplOnGpu::FinishPaintCurrentFrame(
    std::unique_ptr<SkDeferredDisplayList> ddl,
    std::unique_ptr<SkDeferredDisplayList> overdraw_ddl,
    std::vector<ImageContext*> image_contexts,
    std::vector<gpu::SyncToken> sync_tokens,
    uint64_t sync_fence_release,
    base::OnceClosure on_finished) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(ddl);
  DCHECK(output_sk_surface());

  if (!MakeCurrent(true /* need_fbo0 */))
    return;

  dependency_->ScheduleGrContextCleanup();

  PullTextureUpdates(std::move(sync_tokens));

  {
    base::Optional<gpu::raster::GrShaderCache::ScopedCacheUse> cache_use;
    if (dependency_->GetGrShaderCache()) {
      cache_use.emplace(dependency_->GetGrShaderCache(),
                        gpu::kInProcessCommandBufferClientId);
    }

    ScopedPromiseImageAccess scoped_promise_image_access(
        this, std::move(image_contexts));
    if (!scoped_promise_image_access.begin_semaphores().empty()) {
      auto result = output_sk_surface()->wait(
          scoped_promise_image_access.begin_semaphores().size(),
          scoped_promise_image_access.begin_semaphores().data());
      DCHECK(result);
    }

    output_sk_surface()->draw(ddl.get());
    ddl = nullptr;

    if (overdraw_ddl) {
      sk_sp<SkSurface> overdraw_surface = SkSurface::MakeRenderTarget(
          gr_context(), overdraw_ddl->characterization(), SkBudgeted::kNo);
      overdraw_surface->draw(overdraw_ddl.get());
      overdraw_ddl = nullptr;

      SkPaint paint;
      sk_sp<SkImage> overdraw_image = overdraw_surface->makeImageSnapshot();

      sk_sp<SkColorFilter> colorFilter = SkiaHelper::MakeOverdrawColorFilter();
      paint.setColorFilter(colorFilter);
      // TODO(xing.xu): move below to the thread where skia record happens.
      output_sk_surface()->getCanvas()->drawImage(overdraw_image.get(), 0, 0,
                                                  &paint);
    }

    if (output_device_->need_swap_semaphore())
      scoped_promise_image_access.end_semaphores().emplace_back();

    GrFlushInfo flush_info = {
        .fFlags = kNone_GrFlushFlags,
        .fNumSemaphores = scoped_promise_image_access.end_semaphores().size(),
        .fSignalSemaphores =
            scoped_promise_image_access.end_semaphores().data(),
    };

    gpu::AddVulkanCleanupTaskForSkiaFlush(vulkan_context_provider_,
                                          &flush_info);
    if (on_finished)
      gpu::AddCleanupTaskForSkiaFlush(std::move(on_finished), &flush_info);

    auto result = output_sk_surface()->flush(
        SkSurface::BackendSurfaceAccess::kPresent, flush_info);
    if (result != GrSemaphoresSubmitted::kYes &&
        !(scoped_promise_image_access.begin_semaphores().empty() &&
          scoped_promise_image_access.end_semaphores().empty())) {
      // TODO(penghuang): handle vulkan device lost.
      DLOG(ERROR) << "output_sk_surface()->flush() failed.";
      return;
    }
    if (output_device_->need_swap_semaphore()) {
      DCHECK(!swap_buffers_semaphore_.isInitialized());
      swap_buffers_semaphore_ =
          scoped_promise_image_access.end_semaphores().back();
      DCHECK(swap_buffers_semaphore_.isInitialized());
    }
  }
  ReleaseFenceSyncAndPushTextureUpdates(sync_fence_release);
}

void SkiaOutputSurfaceImplOnGpu::SwapBuffers(OutputSurfaceFrame frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(output_sk_surface());
  if (!MakeCurrent(!dependency_->IsOffscreen() /* need_fbo0 */))
    return;

  DCHECK(output_device_);
  gfx::SwapResponse response;
  if (frame.sub_buffer_rect && frame.sub_buffer_rect->IsEmpty()) {
    // TODO(https://crbug.com/898680): Maybe do something for overlays here.
    // This codepath was added in https://codereview.chromium.org/1489153002
    // to support updating overlays without changing the framebuffer contents.
  } else if (capabilities().supports_post_sub_buffer && frame.sub_buffer_rect) {
    if (!capabilities().flipped_output_surface)
      frame.sub_buffer_rect->set_y(size_.height() - frame.sub_buffer_rect->y() -
                                   frame.sub_buffer_rect->height());
    response = output_device_->PostSubBuffer(*frame.sub_buffer_rect,
                                             swap_buffers_semaphore_,
                                             buffer_presented_callback_);
  } else {
    response = output_device_->SwapBuffers(swap_buffers_semaphore_,
                                           buffer_presented_callback_);
  }
  swap_buffers_semaphore_ = GrBackendSemaphore();

  for (auto& latency : frame.latency_info) {
    latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT, response.timings.swap_start,
        1);
    latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_FRAME_SWAP_COMPONENT, response.timings.swap_end,
        1);
  }
  latency_tracker_.OnGpuSwapBuffersCompleted(frame.latency_info);
}

void SkiaOutputSurfaceImplOnGpu::FinishPaintRenderPass(
    RenderPassId id,
    std::unique_ptr<SkDeferredDisplayList> ddl,
    std::vector<ImageContext*> image_contexts,
    std::vector<gpu::SyncToken> sync_tokens,
    uint64_t sync_fence_release) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(ddl);

  if (!MakeCurrent(true /* need_fbo0 */))
    return;

  PullTextureUpdates(std::move(sync_tokens));

  auto& offscreen = offscreen_surfaces_[id];
  SkSurfaceCharacterization characterization;
  if (!offscreen.surface() ||
      !offscreen.surface()->characterize(&characterization) ||
      characterization != ddl->characterization()) {
    offscreen.set_surface(SkSurface::MakeRenderTarget(
        gr_context(), ddl->characterization(), SkBudgeted::kNo));
    DCHECK(offscreen.surface());
  }
  {
    base::Optional<gpu::raster::GrShaderCache::ScopedCacheUse> cache_use;
    if (dependency_->GetGrShaderCache()) {
      cache_use.emplace(dependency_->GetGrShaderCache(),
                        gpu::kInProcessCommandBufferClientId);
    }
    ScopedPromiseImageAccess scoped_promise_image_access(
        this, std::move(image_contexts));
    if (!scoped_promise_image_access.begin_semaphores().empty()) {
      auto result = offscreen.surface()->wait(
          scoped_promise_image_access.begin_semaphores().size(),
          scoped_promise_image_access.begin_semaphores().data());
      DCHECK(result);
    }
    offscreen.surface()->draw(ddl.get());
    GrFlushInfo flush_info = {
        .fFlags = kNone_GrFlushFlags,
        .fNumSemaphores = scoped_promise_image_access.end_semaphores().size(),
        .fSignalSemaphores =
            scoped_promise_image_access.end_semaphores().data(),
    };

    gpu::AddVulkanCleanupTaskForSkiaFlush(vulkan_context_provider_,
                                          &flush_info);
    auto result = offscreen.surface()->flush(
        SkSurface::BackendSurfaceAccess::kNoAccess, flush_info);
    if (result != GrSemaphoresSubmitted::kYes &&
        !(scoped_promise_image_access.begin_semaphores().empty() &&
          scoped_promise_image_access.end_semaphores().empty())) {
      // TODO(penghuang): handle vulkan device lost.
      DLOG(ERROR) << "offscreen.surface()->flush() failed.";
      return;
    }
  }
  ReleaseFenceSyncAndPushTextureUpdates(sync_fence_release);
}

void SkiaOutputSurfaceImplOnGpu::RemoveRenderPassResource(
    std::vector<std::unique_ptr<ImageContext>> image_contexts) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!image_contexts.empty());
  for (auto& image_context : image_contexts) {
    auto it = offscreen_surfaces_.find(image_context->render_pass_id);
    DCHECK(it != offscreen_surfaces_.end());
    offscreen_surfaces_.erase(it);
  }
}

void SkiaOutputSurfaceImplOnGpu::CopyOutput(
    RenderPassId id,
    const copy_output::RenderPassGeometry& geometry,
    const gfx::ColorSpace& color_space,
    std::unique_ptr<CopyOutputRequest> request) {
  // TODO(crbug.com/898595): Do this on the GPU instead of CPU with Vulkan.
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  bool from_fbo0 = !id;
  if (!MakeCurrent(true /* need_fbo0 */))
    return;

  DCHECK(from_fbo0 ||
         offscreen_surfaces_.find(id) != offscreen_surfaces_.end());
  auto* surface =
      from_fbo0 ? output_sk_surface() : offscreen_surfaces_[id].surface();

  if (!is_using_vulkan() && !features::IsUsingSkiaForGLReadback()) {
    // Lazy initialize GLRendererCopier.
    if (!copier_) {
      auto client = std::make_unique<DirectContextProviderDelegateImpl>(
          gpu_preferences_, dependency_->GetGpuDriverBugWorkarounds(),
          dependency_->GetGpuFeatureInfo(), context_state_.get(),
          dependency_->GetMailboxManager(),
          dependency_->GetSharedImageManager(),
          CreateSyncPointClientState(dependency_));
      context_provider_ = base::MakeRefCounted<DirectContextProvider>(
          context_state_->context(), gl_surface_, supports_alpha_,
          gpu_preferences_, feature_info_.get(), std::move(client));
      auto result = context_provider_->BindToCurrentThread();
      if (result != gpu::ContextResult::kSuccess) {
        DLOG(ERROR) << "Couldn't initialize GLRendererCopier";
        context_provider_ = nullptr;
        return;
      }
      context_current_task_runner_ =
          base::MakeRefCounted<ContextCurrentTaskRunner>(this);
      texture_deleter_ =
          std::make_unique<TextureDeleter>(context_current_task_runner_);
      copier_ = std::make_unique<GLRendererCopier>(context_provider_,
                                                   texture_deleter_.get());
      copier_->set_async_gl_task_runner(context_current_task_runner_);
    }
    surface->flush();

    GLuint gl_id = 0;
    GLenum internal_format = supports_alpha_ ? GL_RGBA : GL_RGB;
    bool flipped = from_fbo0 ? !capabilities().flipped_output_surface : false;

    base::Optional<ScopedSurfaceToTexture> texture_mapper;
    if (!from_fbo0 || dependency_->IsOffscreen()) {
      texture_mapper.emplace(context_provider_.get(), surface);
      gl_id = texture_mapper.value().client_id();
      internal_format = GL_RGBA;
    }
    gfx::Size surface_size(surface->width(), surface->height());
    ScopedUseContextProvider use_context_provider(this, gl_id);
    copier_->CopyFromTextureOrFramebuffer(std::move(request), geometry,
                                          internal_format, gl_id, surface_size,
                                          flipped, color_space);

    if (decoder()->HasMoreIdleWork() || decoder()->HasPendingQueries())
      ScheduleDelayedWork();

    return;
  }

  if (request->result_format() ==
      CopyOutputRequest::ResultFormat::I420_PLANES) {
    base::Optional<gpu::raster::GrShaderCache::ScopedCacheUse> cache_use;
    if (dependency_->GetGrShaderCache()) {
      cache_use.emplace(dependency_->GetGrShaderCache(),
                        gpu::kInProcessCommandBufferClientId);
    }
    // For downscaling, use the GOOD quality setting (appropriate for
    // thumbnailing); and, for upscaling, use the BEST quality.
    bool is_downscale_in_both_dimensions =
        request->scale_to().x() < request->scale_from().x() &&
        request->scale_to().y() < request->scale_from().y();
    SkFilterQuality filter_quality = is_downscale_in_both_dimensions
                                         ? kMedium_SkFilterQuality
                                         : kHigh_SkFilterQuality;
    SkIRect srcRect = SkIRect::MakeXYWH(
        geometry.sampling_bounds.x(), geometry.sampling_bounds.y(),
        geometry.sampling_bounds.width(), geometry.sampling_bounds.height());
    std::unique_ptr<ReadPixelsContext> context =
        std::make_unique<ReadPixelsContext>(std::move(request),
                                            geometry.result_bounds);
    surface->asyncRescaleAndReadPixelsYUV420(
        kRec709_SkYUVColorSpace, SkColorSpace::MakeSRGB(), srcRect,
        geometry.result_bounds.width(), geometry.result_bounds.height(),
        SkSurface::RescaleGamma::kSrc, filter_quality, OnYUVReadbackDone,
        context.release());
    return;
  }

  SkBitmap bitmap;
  if (request->is_scaled()) {
    SkImageInfo sampling_bounds_info = SkImageInfo::Make(
        geometry.sampling_bounds.width(), geometry.sampling_bounds.height(),
        SkColorType::kN32_SkColorType, SkAlphaType::kPremul_SkAlphaType,
        surface->getCanvas()->imageInfo().refColorSpace());
    bitmap.allocPixels(sampling_bounds_info);
    surface->readPixels(bitmap, geometry.sampling_bounds.x(),
                        geometry.sampling_bounds.y());

    // Execute the scaling: For downscaling, use the RESIZE_BETTER strategy
    // (appropriate for thumbnailing); and, for upscaling, use the RESIZE_BEST
    // strategy. Note that processing is only done on the subset of the
    // RenderPass output that contributes to the result.
    using skia::ImageOperations;
    const bool is_downscale_in_both_dimensions =
        request->scale_to().x() < request->scale_from().x() &&
        request->scale_to().y() < request->scale_from().y();
    const ImageOperations::ResizeMethod method =
        is_downscale_in_both_dimensions ? ImageOperations::RESIZE_BETTER
                                        : ImageOperations::RESIZE_BEST;
    bitmap = ImageOperations::Resize(
        bitmap, method, geometry.result_bounds.width(),
        geometry.result_bounds.height(),
        SkIRect{geometry.result_selection.x(), geometry.result_selection.y(),
                geometry.result_selection.right(),
                geometry.result_selection.bottom()});
  } else {
    SkImageInfo sampling_bounds_info = SkImageInfo::Make(
        geometry.result_selection.width(), geometry.result_selection.height(),
        SkColorType::kN32_SkColorType, SkAlphaType::kPremul_SkAlphaType,
        surface->getCanvas()->imageInfo().refColorSpace());
    bitmap.allocPixels(sampling_bounds_info);
    surface->readPixels(bitmap, geometry.readback_offset.x(),
                        geometry.readback_offset.y());
  }

  // TODO(crbug.com/795132): Plumb color space throughout SkiaRenderer up to the
  // the SkSurface/SkImage here. Until then, play "musical chairs" with the
  // SkPixelRef to hack-in the RenderPass's |color_space|.
  sk_sp<SkPixelRef> pixels(SkSafeRef(bitmap.pixelRef()));
  SkIPoint origin = bitmap.pixelRefOrigin();
  bitmap.setInfo(bitmap.info().makeColorSpace(color_space.ToSkColorSpace()),
                 bitmap.rowBytes());
  bitmap.setPixelRef(std::move(pixels), origin.x(), origin.y());

  // Deliver the result. SkiaRenderer supports RGBA_BITMAP and I420_PLANES
  // only. For legacy reasons, if a RGBA_TEXTURE request is being made, clients
  // are prepared to accept RGBA_BITMAP results.
  //
  // TODO(crbug/754872): Get rid of the legacy behavior and send empty results
  // for RGBA_TEXTURE requests once tab capture is moved into VIZ.
  const CopyOutputResult::Format result_format =
      (request->result_format() == CopyOutputResult::Format::RGBA_TEXTURE)
          ? CopyOutputResult::Format::RGBA_BITMAP
          : request->result_format();
  // Note: The CopyOutputSkBitmapResult automatically provides I420 format
  // conversion, if needed.
  request->SendResult(std::make_unique<CopyOutputSkBitmapResult>(
      result_format, geometry.result_selection, bitmap));
}

gpu::DecoderContext* SkiaOutputSurfaceImplOnGpu::decoder() {
  return context_provider_->decoder();
}

void SkiaOutputSurfaceImplOnGpu::ScheduleDelayedWork() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (delayed_work_pending_)
    return;
  delayed_work_pending_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SkiaOutputSurfaceImplOnGpu::PerformDelayedWork,
                     weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(2));
}

void SkiaOutputSurfaceImplOnGpu::PerformDelayedWork() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ScopedUseContextProvider use_context_provider(this, /*texture_client_id=*/0);

  delayed_work_pending_ = false;
  if (MakeCurrent(true /* need_fbo0 */)) {
    decoder()->PerformIdleWork();
    decoder()->ProcessPendingQueries(false);
    if (decoder()->HasMoreIdleWork() || decoder()->HasPendingQueries()) {
      ScheduleDelayedWork();
    }
  }
}

// TODO(backer): Add memory tracking.
void SkiaOutputSurfaceImplOnGpu::CreateFallbackImage(ImageContext* context) {
  SkColorType color_type = ResourceFormatToClosestSkColorType(
      true /* gpu_compositing */, context->resource_format);

  // Don't use a fallback SkColorType because we may fail checks inside Skia
  if (SkColorTypeBytesPerPixel(color_type) * 8 !=
      BitsPerPixel(context->resource_format))
    return;

  context->fallback_texture =
      context_state_->gr_context()->createBackendTexture(
          context->size.width(), context->size.height(), color_type,
#if DCHECK_IS_ON()
          SkColors::kRed,
#else
          SkColors::kWhite,
#endif
          GrMipMapped::kNo, GrRenderable::kYes);
  if (!context->fallback_texture.isValid()) {
    DLOG(ERROR) << "Could not create backend texture.";
    return;
  }
  context->promise_image_texture =
      SkPromiseImageTexture::Make(context->fallback_texture);
}

void SkiaOutputSurfaceImplOnGpu::BeginAccessImages(
    const std::vector<ImageContext*>& image_contexts,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (auto* context : image_contexts) {
    // Skip the context if it has been processed.
    if (context->representation_is_being_accessed)
      continue;
    // Prepare for accessing render pass.
    if (context->render_pass_id) {
      // We don't cache promise image for render pass, so the it should always
      // be nullptr.
      auto it = offscreen_surfaces_.find(context->render_pass_id);
      DCHECK(it != offscreen_surfaces_.end());
      context->promise_image_texture = it->second.fulfill();
      if (!context->promise_image_texture) {
        DLOG(ERROR) << "Failed to fulfill the promise texture created from "
                       "RenderPassId:"
                    << context->render_pass_id;
        CreateFallbackImage(context);
      }
      continue;
    }

    // Prepare for accessing shared image.
    if (context->mailbox.IsSharedImage()) {
      if (context->promise_image_texture) {
        // The promise image has been fulfilled early, so we just need begin
        // access the shared image representation. If representation is nullptr,
        // it means the promise image was not fulfilled successfully last time
        // and a fallback content has been used, in that case, we cannot change
        // the content, so we do nothing.
        if (context->representation) {
          auto promise_image_texture = context->representation->BeginReadAccess(
              begin_semaphores, end_semaphores);
          // The image has been fulfilled and cached. It is too late to tell
          // skia the backing of the cached image is not accessible right now,
          // so crash for now.
          // TODO(penghuang): find a way to notify skia.
          CHECK(promise_image_texture);
          context->representation_is_being_accessed = true;
        }
        continue;
      }

      auto representation = shared_image_representation_factory_->ProduceSkia(
          context->mailbox, context_state_.get());
      if (!representation) {
        DLOG(ERROR) << "Failed to fulfill the promise texture - SharedImage "
                       "mailbox not found in SharedImageManager.";
        CreateFallbackImage(context);
        continue;
      }

      if (!(representation->usage() & gpu::SHARED_IMAGE_USAGE_DISPLAY)) {
        DLOG(ERROR) << "Failed to fulfill the promise texture - SharedImage "
                       "was not created with display usage.";
        CreateFallbackImage(context);
        continue;
      }

      if (representation->size() != context->size) {
        DLOG(ERROR) << "Failed to fulfill the promise texture - SharedImage "
                       "size does not match TransferableResource size.";
        CreateFallbackImage(context);
        continue;
      }

      context->representation = std::move(representation);
      context->promise_image_texture = context->representation->BeginReadAccess(
          begin_semaphores, end_semaphores);
      if (!context->promise_image_texture) {
        DLOG(ERROR) << "Failed to fulfill the promise texture - SharedImage "
                       "begin read access failed..";
        CreateFallbackImage(context);
        continue;
      }
      context->representation_is_being_accessed = true;
      continue;
    }

    // Prepare for accessing legacy mailbox.
    // The promise image has been fulfilled once, so we do need do anything.
    if (context->promise_image_texture)
      continue;

    if (is_using_vulkan()) {
      // Probably this texture is created with wrong interface
      // (GLES2Interface).
      DLOG(ERROR)
          << "Failed to fulfill the promise texture whose backend is not "
             "compatible with vulkan.";
      CreateFallbackImage(context);
      continue;
    }

    auto* texture_base =
        dependency_->GetMailboxManager()->ConsumeTexture(context->mailbox);
    if (!texture_base) {
      DLOG(ERROR) << "Failed to fulfill the promise texture.";
      CreateFallbackImage(context);
      continue;
    }

    gfx::Size texture_size;
    if (BindOrCopyTextureIfNecessary(texture_base, &texture_size) &&
        texture_size != context->size) {
      DLOG(ERROR) << "Failed to fulfill the promise texture - texture "
                     "size does not match TransferableResource size.";
      CreateFallbackImage(context);
      continue;
    }

    GrBackendTexture backend_texture;
    gpu::GetGrBackendTexture(gl_version_info_, texture_base->target(),
                             context->size, texture_base->service_id(),
                             context->resource_format, &backend_texture);
    if (!backend_texture.isValid()) {
      DLOG(ERROR) << "Failed to fulfill the promise texture.";
      CreateFallbackImage(context);
      continue;
    }
    context->promise_image_texture =
        SkPromiseImageTexture::Make(backend_texture);
  }
}

void SkiaOutputSurfaceImplOnGpu::EndAccessImages(
    const std::vector<ImageContext*>& image_contexts) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (auto* context : image_contexts) {
    if (!context->representation_is_being_accessed)
      continue;
    // TODO(penghuang): create end read access semaphores and call
    // EndReadAccess() with them. https://crbug.com/944194
    DCHECK(context->representation);
    context->representation->EndReadAccess();
    context->representation_is_being_accessed = false;
  }
}

void SkiaOutputSurfaceImplOnGpu::SetDrawRectangle(
    const gfx::Rect& draw_rectangle) {
  output_device_->SetDrawRectangle(draw_rectangle);
}

sk_sp<GrContextThreadSafeProxy>
SkiaOutputSurfaceImplOnGpu::GetGrContextThreadSafeProxy() {
  return gr_context()->threadSafeProxy();
}

void SkiaOutputSurfaceImplOnGpu::ReleaseImageContexts(
    std::vector<std::unique_ptr<ImageContext>> image_contexts) {
  DCHECK(!image_contexts.empty());
  // The window could be destroyed already, and the MakeCurrent will fail with
  // an destroyed window, so MakeCurrent without requiring the fbo0.
  MakeCurrent(false /* need_fbo0 */);

  for (const auto& image_context : image_contexts) {
    if (image_context->fallback_texture.isValid())
      gpu::DeleteGrBackendTexture(context_state_.get(),
                                  &image_context->fallback_texture);
  }
}

void SkiaOutputSurfaceImplOnGpu::SetCapabilitiesForTesting(
    const OutputSurface::Capabilities& capabilities) {
  MakeCurrent(false /* need_fbo0 */);
  // Check that we're using an offscreen surface.
  DCHECK(dependency_->IsOffscreen());
  output_device_ = std::make_unique<SkiaOutputDeviceOffscreen>(
      gr_context(), capabilities.flipped_output_surface,
      renderer_settings_.requires_alpha_channel,
      did_swap_buffer_complete_callback_);
}

void SkiaOutputSurfaceImplOnGpu::InitializeForGL() {
  std::unique_ptr<SkiaOutputDeviceGL> onscreen_device;
  if (!dependency_->IsOffscreen()) {
    onscreen_device = std::make_unique<SkiaOutputDeviceGL>(
        dependency_, feature_info_, did_swap_buffer_complete_callback_);
    gl_surface_ = onscreen_device->gl_surface();
  } else {
    gl_surface_ = dependency_->CreateGLSurface(nullptr);
  }
  DCHECK(gl_surface_);
  DCHECK_EQ(gl_surface_->IsOffscreen(), dependency_->IsOffscreen());

  context_state_ = dependency_->GetSharedContextState();
  if (!context_state_) {
    LOG(FATAL) << "Failed to create GrContext";
    // TODO(penghuang): handle the failure.
  }

  auto* context = context_state_->real_context();
  auto* current_gl = context->GetCurrentGL();
  api_ = current_gl->Api;
  gl_version_info_ = context->GetVersionInfo();

  if (onscreen_device) {
    if (!MakeCurrent(true /* need_fbo0 */)) {
      LOG(FATAL) << "Failed to make current during initialization.";
      return;
    }
    onscreen_device->Initialize(gr_context(), context);
    supports_alpha_ = onscreen_device->supports_alpha();
    output_device_ = std::move(onscreen_device);
  } else {
    output_device_ = std::make_unique<SkiaOutputDeviceOffscreen>(
        gr_context(), true /* flipped */,
        renderer_settings_.requires_alpha_channel,
        did_swap_buffer_complete_callback_);
    supports_alpha_ = renderer_settings_.requires_alpha_channel;
  }
}

void SkiaOutputSurfaceImplOnGpu::InitializeForVulkan() {
  context_state_ = dependency_->GetSharedContextState();
  DCHECK(context_state_);
#if BUILDFLAG(ENABLE_VULKAN)
  if (dependency_->IsOffscreen()) {
    output_device_ = std::make_unique<SkiaOutputDeviceOffscreen>(
        gr_context(), false /* flipped */,
        renderer_settings_.requires_alpha_channel,
        did_swap_buffer_complete_callback_);
    supports_alpha_ = renderer_settings_.requires_alpha_channel;
  } else {
#if defined(USE_X11)
    supports_alpha_ = true;
    if (gpu_preferences_.disable_vulkan_surface) {
      output_device_ = std::make_unique<SkiaOutputDeviceX11>(
          gr_context(), dependency_->GetSurfaceHandle(),
          did_swap_buffer_complete_callback_);
    } else {
      output_device_ = std::make_unique<SkiaOutputDeviceVulkan>(
          vulkan_context_provider_, dependency_->GetSurfaceHandle(),
          did_swap_buffer_complete_callback_);
    }
#else
    output_device_ = std::make_unique<SkiaOutputDeviceVulkan>(
        vulkan_context_provider_, dependency_->GetSurfaceHandle(),
        did_swap_buffer_complete_callback_);
#endif
  }
#endif
}

bool SkiaOutputSurfaceImplOnGpu::BindOrCopyTextureIfNecessary(
    gpu::TextureBase* texture_base,
    gfx::Size* size) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (texture_base->GetType() != gpu::TextureBase::Type::kValidated)
    return false;
  // If a texture is validated and bound to an image, we may defer copying the
  // image to the texture until the texture is used. It is for implementing low
  // latency drawing (e.g. fast ink) and avoiding unnecessary texture copy. So
  // we need check the texture image state, and bind or copy the image to the
  // texture if necessary.
  auto* texture = gpu::gles2::Texture::CheckedCast(texture_base);
  gpu::gles2::Texture::ImageState image_state;
  auto* image = texture->GetLevelImage(GL_TEXTURE_2D, 0, &image_state);
  if (image && image_state == gpu::gles2::Texture::UNBOUND) {
    glBindTexture(texture_base->target(), texture_base->service_id());
    if (image->ShouldBindOrCopy() == gl::GLImage::BIND) {
      if (!image->BindTexImage(texture_base->target())) {
        LOG(ERROR) << "Failed to bind a gl image to texture.";
        return false;
      }
    } else {
      texture->SetLevelImageState(texture_base->target(), 0,
                                  gpu::gles2::Texture::COPIED);
      if (!image->CopyTexImage(texture_base->target())) {
        LOG(ERROR) << "Failed to copy a gl image to texture.";
        return false;
      }
    }
  }
  GLsizei temp_width, temp_height;
  texture->GetLevelSize(texture_base->target(), 0 /* level */, &temp_width,
                        &temp_height, nullptr /* depth */);
  *size = gfx::Size(temp_width, temp_height);
  return true;
}

bool SkiaOutputSurfaceImplOnGpu::MakeCurrent(bool need_fbo0) {
  if (!is_using_vulkan()) {
    // Only make current with |gl_surface_|, if following operations will use
    // fbo0.
    if (!context_state_->MakeCurrent(need_fbo0 ? gl_surface_.get() : nullptr)) {
      LOG(ERROR) << "Failed to make current.";
      context_lost_callback_.Run();
      if (context_provider_)
        context_provider_->MarkContextLost();
      return false;
    }
    context_state_->set_need_context_state_reset(true);
  }
  return true;
}

void SkiaOutputSurfaceImplOnGpu::PullTextureUpdates(
    std::vector<gpu::SyncToken> sync_tokens) {
  // TODO(https://crbug.com/900973): Remove it when MailboxManager is replaced
  // with SharedImage API.
  if (dependency_->GetMailboxManager()->UsesSync()) {
    for (auto& sync_token : sync_tokens)
      dependency_->GetMailboxManager()->PullTextureUpdates(sync_token);
  }
}

void SkiaOutputSurfaceImplOnGpu::ReleaseFenceSyncAndPushTextureUpdates(
    uint64_t sync_fence_release) {
  // TODO(https://crbug.com/900973): Remove it when MailboxManager is replaced
  // with SharedImage API.
  if (dependency_->GetMailboxManager()->UsesSync()) {
    // If MailboxManagerSync is used, we are sharing textures between threads.
    // In this case, sync point can only guarantee GL commands are issued in
    // correct order across threads and GL contexts. However GPU driver may
    // execute GL commands out of the issuing order across GL contexts. So we
    // have to use PushTextureUpdates() and PullTextureUpdates() to ensure the
    // correct GL commands executing order.
    // PushTextureUpdates(token) will insert a GL fence into the current GL
    // context, PullTextureUpdates(token) will wait the GL fence associated with
    // the give token on the current GL context.
    // Reconstruct sync_token from sync_fence_release.
    gpu::SyncToken sync_token(
        gpu::CommandBufferNamespace::VIZ_SKIA_OUTPUT_SURFACE,
        command_buffer_id(), sync_fence_release);
    dependency_->GetMailboxManager()->PushTextureUpdates(sync_token);
  }
  sync_point_client_state_->ReleaseFenceSync(sync_fence_release);
}

}  // namespace viz
