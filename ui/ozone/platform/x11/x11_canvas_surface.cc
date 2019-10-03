// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_canvas_surface.h"

#include "base/bind.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/vsync_provider.h"

namespace ui {

X11CanvasSurface::X11CanvasSurface(gfx::AcceleratedWidget widget,
                                   base::TaskRunner* gpu_task_runner)
    : task_runner_(base::SequencedTaskRunnerHandle::Get()),
      x11_software_bitmap_presenter_(widget,
                                     task_runner_.get(),
                                     gpu_task_runner) {}

X11CanvasSurface::~X11CanvasSurface() = default;

sk_sp<SkSurface> X11CanvasSurface::GetSurface() {
  DCHECK(surface_);
  return surface_;
}

void X11CanvasSurface::ResizeCanvas(const gfx::Size& viewport_size) {
  if (viewport_pixel_size_ == viewport_size)
    return;
  viewport_pixel_size_ = viewport_size;

  SkCanvas* sk_canvas = nullptr;
  // If the software painter was able to resize the shm pool, use the sk_canvas
  // it can create from the bitmap to create an SkSurface.
  if (x11_software_bitmap_presenter_.Resize(viewport_size))
    sk_canvas = x11_software_bitmap_presenter_.GetSkCanvas();
  CreateSkSurface(sk_canvas);
}

void X11CanvasSurface::PresentCanvas(const gfx::Rect& damage) {
  x11_software_bitmap_presenter_.EndPaint(surface_, damage);
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
  if (x11_software_bitmap_presenter_.ShmPoolReady()) {
    x11_software_bitmap_presenter_.OnSwapBuffers(std::move(swap_ack_callback));
  } else {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(swap_ack_callback), viewport_pixel_size_));
  }
}

int X11CanvasSurface::MaxFramesPending() const {
  return x11_software_bitmap_presenter_.MaxFramesPending();
}

void X11CanvasSurface::CreateSkSurface(SkCanvas* sk_canvas) {
  SkImageInfo info =
      SkImageInfo::MakeN32(viewport_pixel_size_.width(),
                           viewport_pixel_size_.height(), kOpaque_SkAlphaType);
  if (!sk_canvas) {
    surface_ = SkSurface::MakeRaster(info);
  } else {
    surface_ = sk_canvas->makeSurface(info);
  }
}

}  // namespace ui
