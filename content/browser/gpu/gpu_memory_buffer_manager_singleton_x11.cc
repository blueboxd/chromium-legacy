// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_memory_buffer_manager_singleton_x11.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"

namespace content {

GpuMemoryBufferManagerSingletonX11::GpuMemoryBufferManagerSingletonX11(
    int client_id)
    : GpuMemoryBufferManagerSingleton(client_id),
      gpu_data_manager_impl_(GpuDataManagerImpl::GetInstance()) {
  gpu_data_manager_impl_->AddObserver(this);
}

GpuMemoryBufferManagerSingletonX11::~GpuMemoryBufferManagerSingletonX11() {
  gpu_data_manager_impl_->RemoveObserver(this);
}

void GpuMemoryBufferManagerSingletonX11::OnGpuExtraInfoUpdate() {
  have_gpu_info_ = true;
  gpu::GpuMemoryBufferConfigurationSet configs;
  for (const auto& config : gpu_data_manager_impl_->GetGpuExtraInfo()
                                .gpu_memory_buffer_support_x11) {
    configs.insert(config);
  }
  SetNativeConfigurations(std::move(configs));
}

}  // namespace content
