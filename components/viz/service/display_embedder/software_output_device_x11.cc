// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/software_output_device_x11.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <memory>
#include <utility>

#include "base/macros.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/base/x/x11_shm_image_pool.h"
#include "ui/base/x/x11_util.h"
#include "ui/base/x/x11_util_internal.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_error_tracker.h"
#include "ui/gfx/x/x11_types.h"

namespace viz {

SoftwareOutputDeviceX11::SoftwareOutputDeviceX11(
    gfx::AcceleratedWidget widget,
    base::TaskRunner* gpu_task_runner)
    : x11_software_bitmap_presenter_(widget,
                                     task_runner_.get(),
                                     gpu_task_runner) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

SoftwareOutputDeviceX11::~SoftwareOutputDeviceX11() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void SoftwareOutputDeviceX11::Resize(const gfx::Size& pixel_size,
                                     float scale_factor) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // See comment in X11SoftwarePainter::Resize
  if (!x11_software_bitmap_presenter_.Resize(pixel_size))
    SoftwareOutputDevice::Resize(pixel_size, scale_factor);
}

SkCanvas* SoftwareOutputDeviceX11::BeginPaint(const gfx::Rect& damage_rect) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  damage_rect_ = damage_rect;

  auto* sk_canvas = x11_software_bitmap_presenter_.GetSkCanvas();

  if (!sk_canvas)
    sk_canvas = SoftwareOutputDevice::BeginPaint(damage_rect);
  return sk_canvas;
}

void SoftwareOutputDeviceX11::EndPaint() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  SoftwareOutputDevice::EndPaint();
  x11_software_bitmap_presenter_.EndPaint(surface_, damage_rect_);
}

void SoftwareOutputDeviceX11::OnSwapBuffers(
    SwapBuffersCallback swap_ack_callback) {
  if (x11_software_bitmap_presenter_.ShmPoolReady())
    x11_software_bitmap_presenter_.OnSwapBuffers(std::move(swap_ack_callback));
  else
    SoftwareOutputDevice::OnSwapBuffers(std::move(swap_ack_callback));
}

int SoftwareOutputDeviceX11::MaxFramesPending() const {
  return x11_software_bitmap_presenter_.MaxFramesPending();
}

}  // namespace viz
