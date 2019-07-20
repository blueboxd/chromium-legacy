// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/software_output_device_x11.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "base/macros.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/base/x/x11_util.h"
#include "ui/base/x/x11_util_internal.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_error_tracker.h"
#include "ui/gfx/x/x11_types.h"

namespace viz {

namespace {

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

struct XImageDeleter {
  void operator()(XImage* image) const { XDestroyImage(image); }
};

// Draw |data| over |widget|'s parent-relative background, and write the
// resulting image to |widget|.  Returns true on success.
bool CompositeBitmap(XDisplay* display,
                     XID widget,
                     int x,
                     int y,
                     int width,
                     int height,
                     int depth,
                     GC gc,
                     const void* data) {
  XClearArea(display, widget, x, y, width, height, false);

  std::unique_ptr<XImage, XImageDeleter> bg;
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

}  // namespace

SoftwareOutputDeviceX11::SoftwareOutputDeviceX11(gfx::AcceleratedWidget widget)
    : widget_(widget), display_(gfx::GetXDisplay()), gc_(nullptr) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  gc_ = XCreateGC(display_, widget_, 0, nullptr);
  if (!XGetWindowAttributes(display_, widget_, &attributes_)) {
    LOG(ERROR) << "XGetWindowAttributes failed for window " << widget_;
    return;
  }
  ui::GetIntProperty(widget_, "CHROMIUM_COMPOSITE_WINDOW", &composite_);
}

SoftwareOutputDeviceX11::~SoftwareOutputDeviceX11() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  XFreeGC(display_, gc_);
}

void SoftwareOutputDeviceX11::EndPaint() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  SoftwareOutputDevice::EndPaint();

  if (!surface_)
    return;

  gfx::Rect rect = damage_rect_;
  rect.Intersect(gfx::Rect(viewport_pixel_size_));
  if (rect.IsEmpty())
    return;

  if (composite_) {
    SkPixmap pixmap;
    surface_->peekPixels(&pixmap);
    if (CompositeBitmap(display_, widget_, rect.x(), rect.y(), rect.width(),
                        rect.height(), attributes_.depth, gc_, pixmap.addr())) {
      return;
    }
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

    SkPixmap skia_pixmap;
    surface_->peekPixels(&skia_pixmap);
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
    // TODO(jbauman): Switch to XShmPutImage since it's async.
    SkPixmap pixmap;
    surface_->peekPixels(&pixmap);
    gfx::PutARGBImage(display_, attributes_.visual, attributes_.depth, widget_,
                      gc_, static_cast<const uint8_t*>(pixmap.addr()),
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

}  // namespace viz
