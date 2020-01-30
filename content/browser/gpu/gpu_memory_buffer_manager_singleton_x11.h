// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_GPU_MEMORY_BUFFER_MANAGER_SINGLETON_X11_H_
#define CONTENT_BROWSER_GPU_GPU_MEMORY_BUFFER_MANAGER_SINGLETON_X11_H_

#include "content/browser/gpu/gpu_memory_buffer_manager_singleton.h"
#include "content/public/browser/gpu_data_manager_observer.h"

namespace content {

class GpuDataManagerImpl;

// With X11, the supported buffer configurations can only be determined at
// runtime, with help from the GPU process.  This class adds functionality for
// updating the supported configuration list when new GPUInfo is received.
class GpuMemoryBufferManagerSingletonX11
    : public GpuMemoryBufferManagerSingleton,
      public GpuDataManagerObserver {
 public:
  explicit GpuMemoryBufferManagerSingletonX11(int client_id);
  ~GpuMemoryBufferManagerSingletonX11() override;

  GpuMemoryBufferManagerSingletonX11(
      const GpuMemoryBufferManagerSingletonX11&) = delete;
  GpuMemoryBufferManagerSingletonX11& operator=(
      const GpuMemoryBufferManagerSingletonX11&) = delete;

 private:
  // GpuDataManagerObserver:
  void OnGpuExtraInfoUpdate() override;

  bool have_gpu_info_ = false;
  GpuDataManagerImpl* gpu_data_manager_impl_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_GPU_MEMORY_BUFFER_MANAGER_SINGLETON_X11_H_
