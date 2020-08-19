// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_software_bitmap_presenter.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <cstring>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/task/current_thread.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/base/x/x11_shm_image_pool.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_error_tracker.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/gfx/x/xproto.h"
#include "ui/gfx/x/xproto_types.h"

namespace ui {

namespace {

constexpr int kMaxFramesPending = 2;

class ScopedPixmap {
 public:
  ScopedPixmap(XDisplay* display, Pixmap pixmap)
      : display_(display), pixmap_(pixmap) {}

  ~ScopedPixmap() {
    if (pixmap_)
      XFreePixmap(display_, pixmap_);
  }

  operator Pixmap() const { return pixmap_; }

 private:
  XDisplay* display_;
  Pixmap pixmap_;
  DISALLOW_COPY_AND_ASSIGN(ScopedPixmap);
};

}  // namespace

// static
bool X11SoftwareBitmapPresenter::CompositeBitmap(XDisplay* display,
                                                 XID widget,
                                                 int x,
                                                 int y,
                                                 int width,
                                                 int height,
                                                 int depth,
                                                 GC gc,
                                                 const void* data) {
  XClearArea(display, widget, x, y, width, height, false);

  ui::XScopedImage bg;
  {
    gfx::X11ErrorTracker ignore_x_errors;
    bg.reset(XGetImage(display, widget, x, y, width, height, AllPlanes,
                       static_cast<int>(x11::ImageFormat::ZPixmap)));
  }

  // XGetImage() may fail if the drawable is a window and the window is not
  // fully in the bounds of its parent.
  if (!bg) {
    ScopedPixmap pixmap(display,
                        XCreatePixmap(display, widget, width, height, depth));
    if (!pixmap)
      return false;

    XGCValues gcv;
    gcv.subwindow_mode = static_cast<int>(x11::SubwindowMode::IncludeInferiors);
    XChangeGC(display, gc, GCSubwindowMode, &gcv);

    XCopyArea(display, widget, pixmap, gc, x, y, width, height, 0, 0);

    gcv.subwindow_mode = static_cast<int>(x11::SubwindowMode::ClipByChildren);
    XChangeGC(display, gc, GCSubwindowMode, &gcv);

    bg.reset(XGetImage(display, pixmap, 0, 0, width, height, AllPlanes,
                       static_cast<int>(x11::ImageFormat::ZPixmap)));
  }

  if (!bg)
    return false;

  SkBitmap bg_bitmap;
  SkImageInfo image_info = SkImageInfo::Make(
      bg->width, bg->height,
      bg->byte_order == static_cast<int>(x11::ImageOrder::LSBFirst)
          ? kBGRA_8888_SkColorType
          : kRGBA_8888_SkColorType,
      kPremul_SkAlphaType);
  if (!bg_bitmap.installPixels(image_info, bg->data, bg->bytes_per_line))
    return false;
  SkCanvas canvas(bg_bitmap);

  SkBitmap fg_bitmap;
  image_info = SkImageInfo::Make(width, height, kBGRA_8888_SkColorType,
                                 kPremul_SkAlphaType);
  if (!fg_bitmap.installPixels(image_info, const_cast<void*>(data), 4 * width))
    return false;
  canvas.drawBitmap(fg_bitmap, 0, 0);
  canvas.flush();

  XPutImage(display, widget, gc, bg.get(), x, y, x, y, width, height);

  return true;
}

X11SoftwareBitmapPresenter::X11SoftwareBitmapPresenter(
    gfx::AcceleratedWidget widget)
    : widget_(static_cast<x11::Window>(widget)),
      connection_(x11::Connection::Get()),
      display_(connection_->display()),
      gc_(nullptr) {
  DCHECK_NE(widget_, x11::Window::None);
  gc_ = XCreateGC(display_, static_cast<uint32_t>(widget_), 0, nullptr);
  memset(&attributes_, 0, sizeof(attributes_));
  if (!XGetWindowAttributes(display_, static_cast<uint32_t>(widget_),
                            &attributes_)) {
    LOG(ERROR) << "XGetWindowAttributes failed for window "
               << static_cast<uint32_t>(widget_);
    return;
  }

  shm_pool_ = std::make_unique<ui::XShmImagePool>(
      connection_, widget_, attributes_.visual, attributes_.depth,
      kMaxFramesPending);

  // TODO(thomasanderson): Avoid going through the X11 server to plumb this
  // property in.
  ui::GetIntProperty(widget_, "CHROMIUM_COMPOSITE_WINDOW", &composite_);
}

X11SoftwareBitmapPresenter::~X11SoftwareBitmapPresenter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (gc_)
    XFreeGC(display_, gc_);
}

bool X11SoftwareBitmapPresenter::ShmPoolReady() const {
  return shm_pool_ && shm_pool_->Ready();
}

void X11SoftwareBitmapPresenter::Resize(const gfx::Size& pixel_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pixel_size == viewport_pixel_size_)
    return;
  viewport_pixel_size_ = pixel_size;
  // Fallback to the non-shm codepath when |composite_| is true, which only
  // happens for status icon windows that are typically 16x16px.  It's possible
  // to add a shm codepath, but it wouldn't be buying much since it would only
  // affect windows that are tiny and infrequently updated.
  if (!composite_ && shm_pool_ && shm_pool_->Resize(pixel_size)) {
    needs_swap_ = false;
    surface_ = nullptr;
  } else {
    SkColorType color_type = ColorTypeForVisual(
        static_cast<x11::VisualId>(attributes_.visual->visualid));
    if (color_type == kUnknown_SkColorType)
      return;
    SkImageInfo info = SkImageInfo::Make(viewport_pixel_size_.width(),
                                         viewport_pixel_size_.height(),
                                         color_type, kOpaque_SkAlphaType);
    surface_ = SkSurface::MakeRaster(info);
  }
}

SkCanvas* X11SoftwareBitmapPresenter::GetSkCanvas() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ShmPoolReady())
    return shm_pool_->CurrentCanvas();
  else if (surface_)
    return surface_->getCanvas();
  return nullptr;
}

void X11SoftwareBitmapPresenter::EndPaint(const gfx::Rect& damage_rect) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  gfx::Rect rect = damage_rect;
  rect.Intersect(gfx::Rect(viewport_pixel_size_));
  if (rect.IsEmpty())
    return;

  SkPixmap skia_pixmap;

  if (ShmPoolReady()) {
    // TODO(thomasanderson): Investigate direct rendering with DRI3 to avoid any
    // unnecessary X11 IPC or buffer copying.
    if (XShmPutImage(display_, static_cast<uint32_t>(widget_), gc_,
                     shm_pool_->CurrentImage(), rect.x(), rect.y(), rect.x(),
                     rect.y(), rect.width(), rect.height(), x11::True)) {
      needs_swap_ = true;
      return;
    }
    skia_pixmap = shm_pool_->CurrentBitmap().pixmap();
  } else if (surface_) {
    surface_->peekPixels(&skia_pixmap);
  }

  if (!skia_pixmap.addr())
    return;

  if (composite_ &&
      CompositeBitmap(display_, static_cast<uint32_t>(widget_), rect.x(),
                      rect.y(), rect.width(), rect.height(), attributes_.depth,
                      gc_, skia_pixmap.addr())) {
    return;
  }

  auto* connection = x11::Connection::Get();
  auto gc = static_cast<x11::GraphicsContext>(XGContextFromGC(gc_));
  DrawPixmap(connection,
             static_cast<x11::VisualId>(attributes_.visual->visualid), widget_,
             gc, skia_pixmap, rect.x(), rect.y(), rect.x(), rect.y(),
             rect.width(), rect.height());

  // We must be running on a UI thread so that the connection will be flushed.
  DCHECK(base::CurrentUIThread::IsSet());
}

void X11SoftwareBitmapPresenter::OnSwapBuffers(
    SwapBuffersCallback swap_ack_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ShmPoolReady() && needs_swap_)
    shm_pool_->SwapBuffers(std::move(swap_ack_callback));
  else
    std::move(swap_ack_callback).Run(viewport_pixel_size_);
  needs_swap_ = false;
}

int X11SoftwareBitmapPresenter::MaxFramesPending() const {
  return kMaxFramesPending;
}

}  // namespace ui
