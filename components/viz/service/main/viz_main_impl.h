// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_MAIN_VIZ_MAIN_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_MAIN_VIZ_MAIN_IMPL_H_

#include <string>

#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "components/discardable_memory/client/client_discardable_shared_memory_manager.h"
#include "components/viz/service/main/viz_compositor_thread_runner.h"
#include "gpu/ipc/in_process_command_buffer.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/privileged/interfaces/gl/gpu_service.mojom.h"
#include "services/viz/privileged/interfaces/viz_main.mojom.h"
#include "ui/gfx/font_render_params.h"

#if defined(USE_OZONE)
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#endif

namespace gpu {
class GpuInit;
class SyncPointManager;
class SharedImageManager;
}  // namespace gpu

namespace service_manager {
class Connector;
}

namespace ukm {
class MojoUkmRecorder;
}

namespace viz {
class GpuServiceImpl;

#if defined(OS_ANDROID)
using CompositorThreadType = base::android::JavaHandlerThread;
#else
using CompositorThreadType = base::Thread;
#endif

class VizMainImpl : public mojom::VizMain {
 public:
  struct LogMessage {
    int severity;
    std::string header;
    std::string message;
  };
  using LogMessages = std::vector<LogMessage>;

  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void OnInitializationFailed() = 0;
    virtual void OnGpuServiceConnection(GpuServiceImpl* gpu_service) = 0;
    virtual void PostCompositorThreadCreated(
        base::SingleThreadTaskRunner* task_runner) = 0;
    virtual void QuitMainMessageLoop() = 0;
  };

  struct ExternalDependencies {
   public:
    ExternalDependencies();
    ExternalDependencies(ExternalDependencies&& other);
    ~ExternalDependencies();

    ExternalDependencies& operator=(ExternalDependencies&& other);

    bool create_display_compositor = false;
    gpu::SyncPointManager* sync_point_manager = nullptr;
    gpu::SharedImageManager* shared_image_manager = nullptr;
    base::WaitableEvent* shutdown_event = nullptr;
    scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner;
    service_manager::Connector* connector = nullptr;

   private:
    DISALLOW_COPY_AND_ASSIGN(ExternalDependencies);
  };

  VizMainImpl(Delegate* delegate,
              ExternalDependencies dependencies,
              std::unique_ptr<gpu::GpuInit> gpu_init);
  // Destruction must happen on the GPU thread.
  ~VizMainImpl() override;

  void SetLogMessagesForHost(LogMessages messages);

  void BindAssociated(
      mojo::PendingAssociatedReceiver<mojom::VizMain> pending_receiver);

#if defined(USE_OZONE)
  bool CanBindInterface(const std::string& interface_name) const;
  void BindInterface(const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle interface_pipe);
#endif

  // mojom::VizMain implementation:
  void CreateGpuService(
      mojo::PendingReceiver<mojom::GpuService> pending_receiver,
      mojo::PendingRemote<mojom::GpuHost> pending_gpu_host,
      discardable_memory::mojom::DiscardableSharedMemoryManagerPtr
          discardable_memory_manager,
      mojo::ScopedSharedBufferHandle activity_flags,
      gfx::FontRenderParams::SubpixelRendering subpixel_rendering) override;
  void CreateFrameSinkManager(mojom::FrameSinkManagerParamsPtr params) override;
  void CreateVizDevTools(mojom::VizDevToolsParamsPtr params) override;

  GpuServiceImpl* gpu_service() { return gpu_service_.get(); }
  const GpuServiceImpl* gpu_service() const { return gpu_service_.get(); }

  // Note that this may be null if viz is running in the browser process and
  // using the ServiceDiscardableSharedMemoryManager.
  discardable_memory::ClientDiscardableSharedMemoryManager*
  discardable_shared_memory_manager() {
    return discardable_shared_memory_manager_.get();
  }

  // Cleanly exits the process.
  void ExitProcess();

 private:
  // Initializes GPU's UkmRecorder if GPU is running in it's own process.
  void CreateUkmRecorderIfNeeded(service_manager::Connector* connector);

  void CreateFrameSinkManagerInternal(mojom::FrameSinkManagerParamsPtr params);

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner() const {
    return io_thread_ ? io_thread_->task_runner()
                      : dependencies_.io_thread_task_runner;
  }

  Delegate* const delegate_;

  const ExternalDependencies dependencies_;

  // The thread that handles IO events for Gpu (if one isn't already provided).
  // |io_thread_| must be ordered above GPU service related variables so it's
  // destroyed after they are.
  std::unique_ptr<base::Thread> io_thread_;

  LogMessages log_messages_;

  std::unique_ptr<gpu::GpuInit> gpu_init_;
  std::unique_ptr<GpuServiceImpl> gpu_service_;

  // This is created for OOP-D only. It allows the display compositor to use
  // InProcessCommandBuffer to send GPU commands to the GPU thread from the
  // compositor thread. This must outlive |viz_compositor_thread_runner_|.
  std::unique_ptr<gpu::CommandBufferTaskExecutor> task_executor_;

  // If the gpu service is not yet ready then we stash pending
  // FrameSinkManagerParams.
  mojom::FrameSinkManagerParamsPtr pending_frame_sink_manager_params_;

  // Runs the VizCompositorThread for the display compositor with OOP-D.
  std::unique_ptr<VizCompositorThreadRunner> viz_compositor_thread_runner_;

  const scoped_refptr<base::SingleThreadTaskRunner> gpu_thread_task_runner_;

  std::unique_ptr<ukm::MojoUkmRecorder> ukm_recorder_;
  mojo::AssociatedReceiver<mojom::VizMain> receiver_{this};

  std::unique_ptr<discardable_memory::ClientDiscardableSharedMemoryManager>
      discardable_shared_memory_manager_;

#if defined(USE_OZONE)
  // Registry for gpu-related interfaces needed by ozone.
  service_manager::BinderRegistry registry_;
#endif

  DISALLOW_COPY_AND_ASSIGN(VizMainImpl);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_MAIN_VIZ_MAIN_IMPL_H_
