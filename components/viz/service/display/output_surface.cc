// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/output_surface.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "ui/gfx/swap_result.h"

namespace viz {

OutputSurface::OutputSurface() = default;

OutputSurface::OutputSurface(scoped_refptr<ContextProvider> context_provider)
    : context_provider_(std::move(context_provider)) {
  DCHECK(context_provider_);
}

OutputSurface::OutputSurface(
    std::unique_ptr<SoftwareOutputDevice> software_device)
    : software_device_(std::move(software_device)) {
  DCHECK(software_device_);
}

OutputSurface::~OutputSurface() = default;

SkiaOutputSurface* OutputSurface::AsSkiaOutputSurface() {
  return nullptr;
}

gpu::SurfaceHandle OutputSurface::GetSurfaceHandle() const {
  return gpu::kNullSurfaceHandle;
}

void OutputSurface::UpdateLatencyInfoOnSwap(
    const gfx::SwapResponse& response,
    std::vector<ui::LatencyInfo>* latency_info) {
  for (auto& latency : *latency_info) {
    latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT, response.timings.swap_start);
    latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_FRAME_SWAP_COMPONENT,
        response.timings.swap_end);
  }
}

void OutputSurface::SetNeedsSwapSizeNotifications(
    bool needs_swap_size_notifications) {
  DCHECK(!needs_swap_size_notifications);
}

base::ScopedClosureRunner OutputSurface::GetCacheBackBufferCb() {
  return base::ScopedClosureRunner();
}

void OutputSurface::SetGpuVSyncCallback(GpuVSyncCallback callback) {
  NOTREACHED();
}

void OutputSurface::SetGpuVSyncEnabled(bool enabled) {
  NOTREACHED();
}

// Only needs implementation for BrowserCompositorOutputSurface.
bool OutputSurface::IsSoftwareMirrorMode() const {
  return false;
}
}  // namespace viz
