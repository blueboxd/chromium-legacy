// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/wayland_buffer_manager_gpu.h"

#include <utility>

#include "base/bind.h"
#include "base/message_loop/message_loop_current.h"
#include "base/process/process.h"
#include "ui/gfx/linux/drm_util_linux.h"
#include "ui/ozone/platform/wayland/gpu/wayland_surface_gpu.h"

namespace ui {

WaylandBufferManagerGpu::WaylandBufferManagerGpu() = default;
WaylandBufferManagerGpu::~WaylandBufferManagerGpu() = default;

void WaylandBufferManagerGpu::StoreBufferFormatsWithModifiers(
    const base::flat_map<::gfx::BufferFormat, std::vector<uint64_t>>&
        buffer_formats_with_modifiers,
    bool supports_dma_buf) {
  DCHECK(supported_buffer_formats_with_modifiers_.empty());
  supported_buffer_formats_with_modifiers_ = buffer_formats_with_modifiers;

#if defined(WAYLAND_GBM)
  if (!supports_dma_buf)
    set_gbm_device(nullptr);
#endif
}

void WaylandBufferManagerGpu::OnBufferSubmitted(gfx::AcceleratedWidget widget,
                                                uint32_t buffer_id,
                                                gfx::SwapResult swap_result) {
  DCHECK_NE(widget, gfx::kNullAcceleratedWidget);
  auto* surface = GetSurface(widget);
  // The surface might be destroyed by the time the swap result is provided.
  if (surface)
    surface->OnSubmission(buffer_id, swap_result);
}

void WaylandBufferManagerGpu::OnBufferPresented(
    gfx::AcceleratedWidget widget,
    uint32_t buffer_id,
    const gfx::PresentationFeedback& feedback) {
  DCHECK_NE(widget, gfx::kNullAcceleratedWidget);
  auto* surface = GetSurface(widget);
  // The surface might be destroyed by the time the presentation feedback is
  // provided.
  if (surface)
    surface->OnPresentation(buffer_id, feedback);
}

void WaylandBufferManagerGpu::RegisterSurface(gfx::AcceleratedWidget widget,
                                              WaylandSurfaceGpu* surface) {
  base::AutoLock scoped_lock(lock_);
  widget_to_surface_map_.emplace(widget, surface);
}

void WaylandBufferManagerGpu::UnregisterSurface(gfx::AcceleratedWidget widget) {
  base::AutoLock scoped_lock(lock_);
  widget_to_surface_map_.erase(widget);
}

WaylandSurfaceGpu* WaylandBufferManagerGpu::GetSurface(
    gfx::AcceleratedWidget widget) {
  base::AutoLock scoped_lock(lock_);

  auto it = widget_to_surface_map_.find(widget);
  if (it != widget_to_surface_map_.end())
    return it->second;
  return nullptr;
}

const std::vector<uint64_t>&
WaylandBufferManagerGpu::GetModifiersForBufferFormat(
    gfx::BufferFormat buffer_format) const {
  auto it = supported_buffer_formats_with_modifiers_.find(buffer_format);
  if (it != supported_buffer_formats_with_modifiers_.end()) {
    return it->second;
  }
  static std::vector<uint64_t> dummy;
  return dummy;
}

uint32_t WaylandBufferManagerGpu::AllocateBufferID() {
  return ++next_buffer_id_;
}

}  // namespace ui
