// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_canvas_surface.h"

#include "base/bind.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/vsync_provider.h"

namespace ui {

X11CanvasSurface::X11CanvasSurface(
    gfx::AcceleratedWidget widget,
    scoped_refptr<base::SequencedTaskRunner> gpu_task_runner)
    : task_runner_(base::SequencedTaskRunnerHandle::Get()),
      x11_software_bitmap_presenter_(widget,
                                     task_runner_.get(),
                                     std::move(gpu_task_runner)) {}

X11CanvasSurface::~X11CanvasSurface() = default;

SkCanvas* X11CanvasSurface::GetCanvas() {
  return x11_software_bitmap_presenter_.GetSkCanvas();
}

void X11CanvasSurface::ResizeCanvas(const gfx::Size& viewport_size) {
  x11_software_bitmap_presenter_.Resize(viewport_size);
}

void X11CanvasSurface::PresentCanvas(const gfx::Rect& damage) {
  x11_software_bitmap_presenter_.EndPaint(damage);
}

std::unique_ptr<gfx::VSyncProvider> X11CanvasSurface::CreateVSyncProvider() {
  // TODO(https://crbug.com/1001498)
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

bool X11CanvasSurface::SupportsAsyncBufferSwap() const {
  return true;
}

void X11CanvasSurface::OnSwapBuffers(SwapBuffersCallback swap_ack_callback) {
  x11_software_bitmap_presenter_.OnSwapBuffers(std::move(swap_ack_callback));
}

int X11CanvasSurface::MaxFramesPending() const {
  return x11_software_bitmap_presenter_.MaxFramesPending();
}

}  // namespace ui
