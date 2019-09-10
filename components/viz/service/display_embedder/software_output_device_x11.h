// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SOFTWARE_OUTPUT_DEVICE_X11_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SOFTWARE_OUTPUT_DEVICE_X11_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/thread_checker.h"
#include "components/viz/service/display/software_output_device.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_types.h"

namespace ui {
class XShmImagePool;
}

namespace viz {

class VIZ_SERVICE_EXPORT SoftwareOutputDeviceX11 : public SoftwareOutputDevice {
 public:
  explicit SoftwareOutputDeviceX11(gfx::AcceleratedWidget widget,
                                   base::TaskRunner* task_runner);

  ~SoftwareOutputDeviceX11() override;

 private:
  // Draw |data| over |widget|'s parent-relative background, and write the
  // resulting image to |widget|.  Returns true on success.
  static bool CompositeBitmap(XDisplay* display,
                              XID widget,
                              int x,
                              int y,
                              int width,
                              int height,
                              int depth,
                              GC gc,
                              const void* data);

  // SoftwareOutputDevice:
  void Resize(const gfx::Size& pixel_size, float scale_factor) override;
  SkCanvas* BeginPaint(const gfx::Rect& damage_rect) override;
  void EndPaint() override;
  void OnSwapBuffers(SwapBuffersCallback swap_ack_callback) override;
  int MaxFramesPending() const override;

  gfx::AcceleratedWidget widget_;
  XDisplay* display_;
  GC gc_;
  XWindowAttributes attributes_;

  // If nonzero, indicates that the widget should be drawn over its
  // parent-relative background.
  int composite_ = 0;

  scoped_refptr<ui::XShmImagePool> shm_pool_;
  bool needs_swap_ = false;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(SoftwareOutputDeviceX11);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SOFTWARE_OUTPUT_DEVICE_X11_H_
