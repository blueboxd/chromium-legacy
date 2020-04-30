// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface_egl_x11.h"

#include "base/threading/thread_task_runner_handle.h"
#include "ui/base/x/x11_display_util.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/xproto.h"
#include "ui/gl/egl_util.h"

namespace gl {

namespace {

class XrandrIntervalOnlyVSyncProvider : public gfx::VSyncProvider {
 public:
  explicit XrandrIntervalOnlyVSyncProvider(Display* display)
      : display_(display), interval_(base::TimeDelta::FromSeconds(1 / 60.)) {}

  void GetVSyncParameters(UpdateVSyncCallback callback) override {
    if (++calls_since_last_update_ >= kCallsBetweenUpdates) {
      calls_since_last_update_ = 0;
      interval_ = ui::GetPrimaryDisplayRefreshIntervalFromXrandr(display_);
    }
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), base::TimeTicks(), interval_));
  }

  bool GetVSyncParametersIfAvailable(base::TimeTicks* timebase,
                                     base::TimeDelta* interval) override {
    return false;
  }
  bool SupportGetVSyncParametersIfAvailable() const override { return false; }
  bool IsHWClock() const override { return false; }

 private:
  Display* const display_ = nullptr;
  base::TimeDelta interval_;
  static const int kCallsBetweenUpdates = 100;
  int calls_since_last_update_ = kCallsBetweenUpdates;
};

}  // namespace

NativeViewGLSurfaceEGLX11::NativeViewGLSurfaceEGLX11(EGLNativeWindowType window)
    : NativeViewGLSurfaceEGL(window, nullptr) {}

bool NativeViewGLSurfaceEGLX11::Initialize(GLSurfaceFormat format) {
  if (!NativeViewGLSurfaceEGL::Initialize(format))
    return false;

  // Query all child windows and store them. ANGLE creates a child window when
  // eglCreateWidnowSurface is called on X11 and expose events from this window
  // need to be received by this class.
  if (auto reply = x11::Connection::Get()->QueryTree({window_}).Sync())
    children_ = std::move(reply->children);

  if (ui::X11EventSource::HasInstance()) {
    dispatcher_set_ = true;
    ui::X11EventSource::GetInstance()->AddXEventDispatcher(this);
  }
  return true;
}

void NativeViewGLSurfaceEGLX11::Destroy() {
  NativeViewGLSurfaceEGL::Destroy();
  if (dispatcher_set_ && ui::X11EventSource::HasInstance())
    ui::X11EventSource::GetInstance()->RemoveXEventDispatcher(this);
}

gfx::SwapResult NativeViewGLSurfaceEGLX11::SwapBuffers(
    PresentationCallback callback) {
  auto result = NativeViewGLSurfaceEGL::SwapBuffers(std::move(callback));
  if (result == gfx::SwapResult::SWAP_FAILED)
    return result;

  // We need to restore the background pixel that we set to WhitePixel on
  // views::DesktopWindowTreeHostX11::InitX11Window back to None for the
  // XWindow associated to this surface after the first SwapBuffers has
  // happened, to avoid showing a weird white background while resizing.
  if (GetXNativeDisplay() && !has_swapped_buffers_) {
    XSetWindowBackgroundPixmap(GetXNativeDisplay(), window_, 0);
    has_swapped_buffers_ = true;
  }
  return result;
}

NativeViewGLSurfaceEGLX11::~NativeViewGLSurfaceEGLX11() {
  Destroy();
}

Display* NativeViewGLSurfaceEGLX11::GetXNativeDisplay() const {
  return reinterpret_cast<Display*>(GetNativeDisplay());
}

std::unique_ptr<gfx::VSyncProvider>
NativeViewGLSurfaceEGLX11::CreateVsyncProviderInternal() {
  return std::make_unique<XrandrIntervalOnlyVSyncProvider>(GetXNativeDisplay());
}

bool NativeViewGLSurfaceEGLX11::DispatchXEvent(XEvent* x_event) {
  // When ANGLE is used for EGL, it creates an X11 child window. Expose events
  // from this window need to be forwarded to this class.
  bool can_dispatch = x_event->type == Expose &&
                      std::find(children_.begin(), children_.end(),
                                x_event->xexpose.window) != children_.end();

  if (!can_dispatch)
    return false;

  x_event->xexpose.window = window_;
  Display* x11_display = GetXNativeDisplay();
  XSendEvent(x11_display, window_, x11::False, ExposureMask, x_event);
  XFlush(x11_display);
  return true;
}

}  // namespace gl
