// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/output_surface_provider_impl.h"

#include <memory>
#include <utility>

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/base/switches.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/service/display_embedder/gl_output_surface.h"
#include "components/viz/service/display_embedder/gl_output_surface_offscreen.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency_impl.h"
#include "components/viz/service/display_embedder/skia_output_surface_impl.h"
#include "components/viz/service/display_embedder/skia_output_surface_impl_non_ddl.h"
#include "components/viz/service/display_embedder/software_output_surface.h"
#include "components/viz/service/display_embedder/viz_process_context_provider.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/command_buffer/service/mailbox_manager_factory.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/ipc/command_buffer_task_executor.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "gpu/ipc/service/image_transport_surface.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/init/gl_factory.h"

#if defined(OS_WIN)
#include "components/viz/service/display_embedder/gl_output_surface_win.h"
#include "components/viz/service/display_embedder/software_output_device_win.h"
#endif

#if defined(OS_ANDROID)
#include "components/viz/service/display_embedder/gl_output_surface_android.h"
#include "components/viz/service/display_embedder/gl_output_surface_buffer_queue_android.h"
#endif

#if defined(OS_MACOSX)
#include "components/viz/service/display_embedder/gl_output_surface_mac.h"
#include "components/viz/service/display_embedder/software_output_device_mac.h"
#include "ui/base/cocoa/remote_layer_api.h"
#endif

#if defined(USE_X11)
#include "components/viz/service/display_embedder/software_output_device_x11.h"
#endif

#if defined(USE_OZONE)
#include "components/viz/service/display_embedder/gl_output_surface_ozone.h"
#include "components/viz/service/display_embedder/software_output_device_ozone.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/platform_window_surface.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#include "ui/ozone/public/surface_ozone_canvas.h"
#endif

namespace viz {

OutputSurfaceProviderImpl::OutputSurfaceProviderImpl(
    GpuServiceImpl* gpu_service_impl,
    gpu::CommandBufferTaskExecutor* task_executor,
    gpu::GpuChannelManagerDelegate* gpu_channel_manager_delegate,
    std::unique_ptr<gpu::GpuMemoryBufferManager> gpu_memory_buffer_manager,
    gpu::ImageFactory* image_factory,
    bool headless)
    : gpu_service_impl_(gpu_service_impl),
      task_executor_(task_executor),
      gpu_channel_manager_delegate_(gpu_channel_manager_delegate),
      gpu_memory_buffer_manager_(std::move(gpu_memory_buffer_manager)),
      image_factory_(image_factory),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      headless_(headless) {}

OutputSurfaceProviderImpl::OutputSurfaceProviderImpl(bool headless)
    : OutputSurfaceProviderImpl(
          /*gpu_service_impl=*/nullptr,
          /*task_executor=*/nullptr,
          /*gpu_channel_manager_delegate=*/nullptr,
          /*gpu_memory_buffer_manager=*/nullptr,
          /*image_factory=*/nullptr,
          headless) {}

OutputSurfaceProviderImpl::~OutputSurfaceProviderImpl() = default;

std::unique_ptr<OutputSurface> OutputSurfaceProviderImpl::CreateOutputSurface(
    gpu::SurfaceHandle surface_handle,
    bool gpu_compositing,
    mojom::DisplayClient* display_client,
    const RendererSettings& renderer_settings) {
  // TODO(penghuang): Merge two output surfaces into one when GLRenderer and
  // software compositor is removed.
  std::unique_ptr<OutputSurface> output_surface;

  if (!gpu_compositing) {
    output_surface = std::make_unique<SoftwareOutputSurface>(
        CreateSoftwareOutputDeviceForPlatform(surface_handle, display_client));
  } else if (renderer_settings.use_skia_renderer ||
             renderer_settings.use_skia_renderer_non_ddl) {
#if defined(OS_MACOSX)
    // TODO(penghuang): Support SkiaRenderer for all platforms.
    NOTIMPLEMENTED();
    return nullptr;
#else
    if (renderer_settings.use_skia_renderer_non_ddl) {
      DCHECK_EQ(gl::GetGLImplementation(), gl::kGLImplementationEGLGLES2)
          << "SkiaRendererNonDDL is only supported with GLES2.";
      auto gl_surface = gpu::ImageTransportSurface::CreateNativeSurface(
          nullptr, surface_handle, gl::GLSurfaceFormat());
      if (!shared_context_state_) {
        auto gl_share_group = base::MakeRefCounted<gl::GLShareGroup>();
        auto gl_context = gl::init::CreateGLContext(
            gl_share_group.get(), gl_surface.get(), gl::GLContextAttribs());
        gl_context->MakeCurrent(gl_surface.get());
        shared_context_state_ = base::MakeRefCounted<gpu::SharedContextState>(
            std::move(gl_share_group), gl_surface, std::move(gl_context),
            false /* use_virtualized_gl_contexts */, base::DoNothing::Once(),
            nullptr /* vulkan_context_provider */);
        shared_context_state_->InitializeGrContext(
            gpu::GpuDriverBugWorkarounds(), nullptr /* gr_shader_cache */);
        mailbox_manager_ = gpu::gles2::CreateMailboxManager(
            gpu_service_impl_->gpu_preferences());
        DCHECK(mailbox_manager_->UsesSync());
      }
      output_surface = std::make_unique<SkiaOutputSurfaceImplNonDDL>(
          std::move(gl_surface), shared_context_state_, mailbox_manager_.get(),
          gpu_service_impl_->shared_image_manager(),
          gpu_service_impl_->sync_point_manager(),
          true /* need_swapbuffers_ack */);

    } else {
      output_surface = std::make_unique<SkiaOutputSurfaceImpl>(
          std::make_unique<SkiaOutputSurfaceDependencyImpl>(gpu_service_impl_,
                                                            surface_handle),
          renderer_settings);
    }
#endif
  } else {
    DCHECK(task_executor_);

    scoped_refptr<VizProcessContextProvider> context_provider;

    // Retry creating and binding |context_provider| on transient failures.
    gpu::ContextResult context_result = gpu::ContextResult::kTransientFailure;
    while (context_result != gpu::ContextResult::kSuccess) {
      // We are about to exit the GPU process so don't try to create a context.
      // It will be recreated after the GPU process restarts. The same check
      // also happens on the GPU thread before the context gets initialized
      // there. If GPU process starts to exit after this check but before
      // context initialization we'll encounter a transient error, loop and hit
      // this check again.
      if (gpu_channel_manager_delegate_->IsExiting())
        return nullptr;

      context_provider = base::MakeRefCounted<VizProcessContextProvider>(
          task_executor_, surface_handle, gpu_memory_buffer_manager_.get(),
          image_factory_, gpu_channel_manager_delegate_, renderer_settings);
      context_result = context_provider->BindToCurrentThread();

#if defined(OS_ANDROID)
      display_client->OnContextCreationResult(context_result);
#endif

      if (IsFatalOrSurfaceFailure(context_result)) {
#if defined(OS_CHROMEOS) || defined(IS_CHROMECAST)
        // TODO(kylechar): Chrome OS can't disable GPU compositing. This needs
        // to be handled similar to Android.
        CHECK(false);
#elif !defined(OS_ANDROID)
        gpu_service_impl_->DisableGpuCompositing();
#endif
        return nullptr;
      }
    }

    if (surface_handle == gpu::kNullSurfaceHandle) {
      output_surface = std::make_unique<GLOutputSurfaceOffscreen>(
          std::move(context_provider));
    } else if (context_provider->ContextCapabilities().surfaceless) {
#if defined(USE_OZONE)
      output_surface = std::make_unique<GLOutputSurfaceOzone>(
          std::move(context_provider), surface_handle,
          gpu_memory_buffer_manager_.get(),
          renderer_settings.overlay_strategies);
#elif defined(OS_MACOSX)
      output_surface = std::make_unique<GLOutputSurfaceMac>(
          std::move(context_provider), surface_handle,
          gpu_memory_buffer_manager_.get(), renderer_settings.allow_overlays);
#elif defined(OS_ANDROID)
      auto buffer_format = context_provider->UseRGB565PixelFormat()
                               ? gfx::BufferFormat::BGR_565
                               : gfx::BufferFormat::RGBA_8888;
      output_surface = std::make_unique<GLOutputSurfaceBufferQueueAndroid>(
          std::move(context_provider), surface_handle,
          gpu_memory_buffer_manager_.get(), buffer_format);
#else
      NOTREACHED();
#endif
    } else {
#if defined(OS_WIN)
      const auto& capabilities = context_provider->ContextCapabilities();
      const bool use_overlays_for_sw_protected_video =
          base::FeatureList::IsEnabled(
              features::kUseDCOverlaysForSoftwareProtectedVideo);
      const bool use_overlays =
          capabilities.dc_layers && (capabilities.use_dc_overlays_for_video ||
                                     use_overlays_for_sw_protected_video);
      output_surface = std::make_unique<GLOutputSurfaceWin>(
          std::move(context_provider), use_overlays);
#elif defined(OS_ANDROID)
      // When SurfaceControl is enabled, any resource backed by an
      // AHardwareBuffer can be marked as an overlay candidate but it requires
      // that we use a SurfaceControl backed GLSurface. If we're creating a
      // native window backed GLSurface, the overlay processing code will
      // incorrectly assume these resources can be overlayed. So we disable all
      // overlay processing for this OutputSurface.
      const bool allow_overlays =
          task_executor_->gpu_feature_info()
              .status_values[gpu::GPU_FEATURE_TYPE_ANDROID_SURFACE_CONTROL] !=
          gpu::kGpuFeatureStatusEnabled;

      output_surface = std::make_unique<GLOutputSurfaceAndroid>(
          std::move(context_provider), allow_overlays);
#else
      output_surface =
          std::make_unique<GLOutputSurface>(std::move(context_provider));
#endif
    }
  }

  return output_surface;
}

std::unique_ptr<SoftwareOutputDevice>
OutputSurfaceProviderImpl::CreateSoftwareOutputDeviceForPlatform(
    gpu::SurfaceHandle surface_handle,
    mojom::DisplayClient* display_client) {
  if (headless_)
    return std::make_unique<SoftwareOutputDevice>();

#if defined(OS_WIN)
  return CreateSoftwareOutputDeviceWin(surface_handle, &output_device_backing_,
                                       display_client);
#elif defined(OS_MACOSX)
  return std::make_unique<SoftwareOutputDeviceMac>(task_runner_);
#elif defined(OS_ANDROID)
  // Android does not do software compositing, so we can't get here.
  NOTREACHED();
  return nullptr;
#elif defined(USE_OZONE)
  ui::SurfaceFactoryOzone* factory =
      ui::OzonePlatform::GetInstance()->GetSurfaceFactoryOzone();
  std::unique_ptr<ui::PlatformWindowSurface> platform_window_surface =
      factory->CreatePlatformWindowSurface(surface_handle);
  std::unique_ptr<ui::SurfaceOzoneCanvas> surface_ozone =
      factory->CreateCanvasForWidget(surface_handle);
  CHECK(surface_ozone);
  return std::make_unique<SoftwareOutputDeviceOzone>(
      std::move(platform_window_surface), std::move(surface_ozone));
#elif defined(USE_X11)
  return std::make_unique<SoftwareOutputDeviceX11>(surface_handle);
#endif
}

}  // namespace viz
