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

#include "base/macros.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/base/x/x11_util.h"
#include "ui/base/x/x11_util_internal.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_error_tracker.h"
#include "ui/gfx/x/x11_types.h"

#if defined(USE_OZONE)
#include "ui/base/x/x11_shm_image_pool_ozone.h"
#else
#include "ui/base/x/x11_shm_image_pool.h"
#endif

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
    bg.reset(
        XGetImage(display, widget, x, y, width, height, AllPlanes, ZPixmap));
  }

  // XGetImage() may fail if the drawable is a window and the window is not
  // fully in the bounds of its parent.
  if (!bg) {
    ScopedPixmap pixmap(display,
                        XCreatePixmap(display, widget, width, height, depth));
    if (!pixmap)
      return false;

    XGCValues gcv;
    gcv.subwindow_mode = IncludeInferiors;
    XChangeGC(display, gc, GCSubwindowMode, &gcv);

    XCopyArea(display, widget, pixmap, gc, x, y, width, height, 0, 0);

    gcv.subwindow_mode = ClipByChildren;
    XChangeGC(display, gc, GCSubwindowMode, &gcv);

    bg.reset(
        XGetImage(display, pixmap, 0, 0, width, height, AllPlanes, ZPixmap));
  }

  if (!bg)
    return false;

  SkBitmap bg_bitmap;
  SkImageInfo image_info =
      SkImageInfo::Make(bg->width, bg->height,
                        bg->byte_order == LSBFirst ? kBGRA_8888_SkColorType
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

  XFlush(display);
  return true;
}

X11SoftwareBitmapPresenter::X11SoftwareBitmapPresenter(
    gfx::AcceleratedWidget widget,
    base::TaskRunner* host_task_runner,
    base::TaskRunner* event_task_runner)
    : widget_(widget), display_(gfx::GetXDisplay()), gc_(nullptr) {
  DCHECK_NE(widget_, gfx::kNullAcceleratedWidget);
  gc_ = XCreateGC(display_, widget_, 0, nullptr);
  memset(&attributes_, 0, sizeof(attributes_));
  if (!XGetWindowAttributes(display_, widget_, &attributes_)) {
    LOG(ERROR) << "XGetWindowAttributes failed for window " << widget_;
    return;
  }

#if defined(USE_OZONE)
  shm_pool_ = base::MakeRefCounted<ui::X11ShmImagePoolOzone>(
      host_task_runner, event_task_runner, display_, widget_,
      attributes_.visual, attributes_.depth, kMaxFramesPending);
#else
  shm_pool_ = base::MakeRefCounted<ui::X11ShmImagePool>(
      host_task_runner, event_task_runner, display_, widget_,
      attributes_.visual, attributes_.depth, kMaxFramesPending);
#endif
  shm_pool_->Initialize();

  // TODO(thomasanderson): Avoid going through the X11 server to plumb this
  // property in.
  ui::GetIntProperty(widget_, "CHROMIUM_COMPOSITE_WINDOW", &composite_);
}

X11SoftwareBitmapPresenter::~X11SoftwareBitmapPresenter() {
  if (gc_)
    XFreeGC(display_, gc_);

  if (shm_pool_)
    shm_pool_->Teardown();
}

bool X11SoftwareBitmapPresenter::ShmPoolReady() const {
  return shm_pool_ && shm_pool_->Ready();
}

bool X11SoftwareBitmapPresenter::Resize(const gfx::Size& pixel_size) {
  viewport_pixel_size_ = pixel_size;
  // Fallback to the non-shm codepath when |composite_| is true, which only
  // happens for status icon windows that are typically 16x16px.  It's possible
  // to add a shm codepath, but it wouldn't be buying much since it would only
  // affect windows that are tiny and infrequently updated.
  if (!composite_ && shm_pool_ && shm_pool_->Resize(pixel_size)) {
    needs_swap_ = false;
    return true;
  }
  return false;
}

SkCanvas* X11SoftwareBitmapPresenter::GetSkCanvas() {
  if (ShmPoolReady())
    return shm_pool_->CurrentCanvas();
  return nullptr;
}

void X11SoftwareBitmapPresenter::EndPaint(sk_sp<SkSurface> sk_surface,
                                          const gfx::Rect& damage_rect) {
  gfx::Rect rect = damage_rect;
  rect.Intersect(gfx::Rect(viewport_pixel_size_));
  if (rect.IsEmpty())
    return;

  SkPixmap skia_pixmap;

  if (ShmPoolReady()) {
    DCHECK(!sk_surface);
    // TODO(thomasanderson): Investigate direct rendering with DRI3 to avoid any
    // unnecessary X11 IPC or buffer copying.
    if (XShmPutImage(display_, widget_, gc_, shm_pool_->CurrentImage(),
                     rect.x(), rect.y(), rect.x(), rect.y(), rect.width(),
                     rect.height(), x11::True)) {
      needs_swap_ = true;
      XFlush(display_);
      return;
    }
    skia_pixmap = shm_pool_->CurrentBitmap().pixmap();
  } else {
    DCHECK(sk_surface);
    sk_surface->peekPixels(&skia_pixmap);
  }

  if (composite_ &&
      CompositeBitmap(display_, widget_, rect.x(), rect.y(), rect.width(),
                      rect.height(), attributes_.depth, gc_,
                      skia_pixmap.addr())) {
    return;
  }

  int bpp = gfx::BitsPerPixelForPixmapDepth(display_, attributes_.depth);

  if (bpp != 32 && bpp != 16 && ui::QueryRenderSupport(display_)) {
    // gfx::PutARGBImage only supports 16 and 32 bpp, but Xrender can do other
    // conversions.
    Pixmap pixmap =
        XCreatePixmap(display_, widget_, rect.width(), rect.height(), 32);
    GC gc = XCreateGC(display_, pixmap, 0, nullptr);
    XImage image;
    memset(&image, 0, sizeof(image));

    image.width = viewport_pixel_size_.width();
    image.height = viewport_pixel_size_.height();
    image.depth = 32;
    image.bits_per_pixel = 32;
    image.format = ZPixmap;
    image.byte_order = LSBFirst;
    image.bitmap_unit = 8;
    image.bitmap_bit_order = LSBFirst;
    image.bytes_per_line = skia_pixmap.rowBytes();
    image.red_mask = 0xff;
    image.green_mask = 0xff00;
    image.blue_mask = 0xff0000;
    image.data =
        const_cast<char*>(static_cast<const char*>(skia_pixmap.addr()));

    XPutImage(display_, pixmap, gc, &image, rect.x(),
              rect.y() /* source x, y */, 0, 0 /* dest x, y */, rect.width(),
              rect.height());
    XFreeGC(display_, gc);
    Picture picture = XRenderCreatePicture(
        display_, pixmap, ui::GetRenderARGB32Format(display_), 0, nullptr);
    XRenderPictFormat* pictformat =
        XRenderFindVisualFormat(display_, attributes_.visual);
    Picture dest_picture =
        XRenderCreatePicture(display_, widget_, pictformat, 0, nullptr);
    XRenderComposite(display_,
                     PictOpSrc,       // op
                     picture,         // src
                     0,               // mask
                     dest_picture,    // dest
                     0,               // src_x
                     0,               // src_y
                     0,               // mask_x
                     0,               // mask_y
                     rect.x(),        // dest_x
                     rect.y(),        // dest_y
                     rect.width(),    // width
                     rect.height());  // height
    XRenderFreePicture(display_, picture);
    XRenderFreePicture(display_, dest_picture);
    XFreePixmap(display_, pixmap);
  } else {
    gfx::PutARGBImage(display_, attributes_.visual, attributes_.depth, widget_,
                      gc_, static_cast<const uint8_t*>(skia_pixmap.addr()),
                      viewport_pixel_size_.width(),
                      viewport_pixel_size_.height(), rect.x(), rect.y(),
                      rect.x(), rect.y(), rect.width(), rect.height());
  }

  // Ensure the new window content appears immediately. On a TYPE_UI thread we
  // can rely on the message loop to flush for us so XFlush() isn't necessary.
  // However, this code can run on a different thread and would have to wait for
  // the TYPE_UI thread to no longer be idle before a flush happens.
  XFlush(display_);
}

void X11SoftwareBitmapPresenter::OnSwapBuffers(
    SwapBuffersCallback swap_ack_callback) {
  DCHECK(ShmPoolReady());
  if (needs_swap_)
    shm_pool_->SwapBuffers(std::move(swap_ack_callback));
  else
    std::move(swap_ack_callback).Run(viewport_pixel_size_);
  needs_swap_ = false;
}

int X11SoftwareBitmapPresenter::MaxFramesPending() const {
  return kMaxFramesPending;
}

}  // namespace ui
