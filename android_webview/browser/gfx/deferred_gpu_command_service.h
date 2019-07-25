// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_DEFERRED_GPU_COMMAND_SERVICE_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_DEFERRED_GPU_COMMAND_SERVICE_H_

#include <stddef.h>

#include <memory>
#include <utility>

#include "android_webview/browser/gfx/gpu_service_web_view.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_local.h"
#include "base/time/time.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/command_buffer_task_executor.h"

namespace android_webview {
class TaskQueueWebView;
class ScopedAllowGL;

// Implementation for gpu service objects accessor for command buffer WebView.
class DeferredGpuCommandService : public gpu::CommandBufferTaskExecutor {
 public:
  static DeferredGpuCommandService* GetInstance();

  // gpu::CommandBufferTaskExecutor implementation.
  bool ForceVirtualizedGLContexts() const override;
  bool ShouldCreateMemoryTracker() const override;
  std::unique_ptr<gpu::SingleTaskSequence> CreateSequence() override;
  void ScheduleOutOfOrderTask(base::OnceClosure task) override;
  void ScheduleDelayedWork(base::OnceClosure task) override;
  void PostNonNestableToClient(base::OnceClosure callback) override;

  const gpu::GPUInfo& gpu_info() const { return gpu_service_->gpu_info(); }

  bool CanSupportThreadedTextureMailbox() const;

 protected:
  ~DeferredGpuCommandService() override;

 private:
  friend class ScopedAllowGL;
  friend class TaskForwardingSequence;

  DeferredGpuCommandService(TaskQueueWebView* task_queue,
                            GpuServiceWebView* gpu_service);

  static DeferredGpuCommandService* CreateDeferredGpuCommandService();

  TaskQueueWebView* task_queue_;
  GpuServiceWebView* gpu_service_;

  DISALLOW_COPY_AND_ASSIGN(DeferredGpuCommandService);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_DEFERRED_GPU_COMMAND_SERVICE_H_
