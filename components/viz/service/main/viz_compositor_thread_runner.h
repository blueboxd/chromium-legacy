// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_MAIN_VIZ_COMPOSITOR_THREAD_RUNNER_H_
#define COMPONENTS_VIZ_SERVICE_MAIN_VIZ_COMPOSITOR_THREAD_RUNNER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_loop.h"
#include "build/build_config.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "services/viz/privileged/interfaces/viz_main.mojom.h"

#if defined(OS_ANDROID)
#include "base/android/java_handler_thread.h"
#endif

namespace base {
class SingleThreadTaskRunner;
class Thread;
}  // namespace base

namespace gpu {
class CommandBufferTaskExecutor;
}  // namespace gpu

namespace ui_devtools {
class UiDevToolsServer;
}  // namespace ui_devtools

namespace viz {
class OutputSurfaceProvider;
class FrameSinkManagerImpl;
class GpuServiceImpl;
class ServerSharedBitmapManager;

#if defined(OS_ANDROID)
using VizCompositorThreadType = base::android::JavaHandlerThread;
#else
using VizCompositorThreadType = base::Thread;
#endif

// Starts and runs the VizCompositorThread. The thread will be started when this
// object is constructed. Objects on the thread will be initialized after
// calling CreateFrameSinkManager(). Destructor will teardown objects on thread
// and then stop the thread.
class VizCompositorThreadRunner {
 public:
  explicit VizCompositorThreadRunner(base::MessageLoop::Type message_loop_type);
  // Performs teardown on thread and then stops thread.
  ~VizCompositorThreadRunner();

  // Returns the TaskRunner for VizCompositorThread.
  base::SingleThreadTaskRunner* task_runner() { return task_runner_.get(); }

  // Creates FrameSinkManager from |params|. The version with |gpu_service| and
  // |task_executor| supports both GPU and software compositing, while the
  // version without supports only software compositing. Should be called from
  // the thread that owns |this| to initialize state on VizCompositorThread.
  void CreateFrameSinkManager(mojom::FrameSinkManagerParamsPtr params);
  void CreateFrameSinkManager(mojom::FrameSinkManagerParamsPtr params,
                              gpu::CommandBufferTaskExecutor* task_executor,
                              GpuServiceImpl* gpu_service);

#if defined(USE_VIZ_DEVTOOLS)
  void CreateVizDevTools(mojom::VizDevToolsParamsPtr params);
#endif

  // Performs cleanup on VizCompositorThread needed before forcing thread to
  // shut down. Ensures VizCompositorThread teardown during the destructor
  // doesn't block on PostTasks back to the GPU thread. After cleanup has
  // finished |cleanup_finished_callback| will be run. Should be called from the
  // thread that owns |this|.
  //
  // This is intended to be used when the GPU thread wants to force restart. The
  // cleanup is normally handled by the browser process before GPU process
  // shutdown, except if the GPU thread is forcing restart.
  void CleanupForShutdown(base::OnceClosure cleanup_finished_callback);

 private:
  void CreateFrameSinkManagerOnCompositorThread(
      mojom::FrameSinkManagerParamsPtr params,
      gpu::CommandBufferTaskExecutor* task_executor,
      GpuServiceImpl* gpu_service);
#if defined(USE_VIZ_DEVTOOLS)
  void CreateVizDevToolsOnCompositorThread(mojom::VizDevToolsParamsPtr params);
  void InitVizDevToolsOnCompositorThread(mojom::VizDevToolsParamsPtr params);
#endif
  void CleanupForShutdownOnCompositorThread();
  void TearDownOnCompositorThread();

  // Start variables to be accessed only on |task_runner_|.
  std::unique_ptr<ServerSharedBitmapManager> server_shared_bitmap_manager_;
  std::unique_ptr<OutputSurfaceProvider> output_surface_provider_;
  std::unique_ptr<FrameSinkManagerImpl> frame_sink_manager_;
#if defined(USE_VIZ_DEVTOOLS)
  std::unique_ptr<ui_devtools::UiDevToolsServer> devtools_server_;

  // If the FrameSinkManager is not ready yet, then we stash the pending
  // VizDevToolsParams.
  mojom::VizDevToolsParamsPtr pending_viz_dev_tools_params_;
#endif
  // End variables to be accessed only on |task_runner_|.

  std::unique_ptr<VizCompositorThreadType> thread_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(VizCompositorThreadRunner);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_MAIN_VIZ_COMPOSITOR_THREAD_RUNNER_H_
