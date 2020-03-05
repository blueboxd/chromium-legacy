// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_glx_native_pixmap.h"

#include <xcb/dri3.h>
#include <xcb/xcb.h>

#include "base/posix/eintr_wrapper.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"
#include "ui/gfx/x/x11.h"
#include "ui/gl/buffer_format_utils.h"
#include "ui/gl/gl_bindings.h"

namespace gl {

namespace {

int Depth(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::BGR_565:
      return 16;
    case gfx::BufferFormat::BGRX_8888:
      return 24;
    case gfx::BufferFormat::BGRA_1010102:
      // It's unclear why this is 32 instead of 30.
      return 32;
    case gfx::BufferFormat::BGRA_8888:
      return 32;
    default:
      NOTREACHED();
      return 0;
  }
}

int Bpp(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::BGR_565:
      return 16;
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::BGRA_1010102:
    case gfx::BufferFormat::BGRA_8888:
      return 32;
    default:
      NOTREACHED();
      return 0;
  }
}

XID XPixmapFromNativePixmap(const gfx::NativePixmapDmaBuf& native_pixmap,
                            int depth,
                            int bpp) {
  XDisplay* display = gfx::GetXDisplay();
  xcb_connection_t* connection = XGetXCBConnection(display);

  int fd = HANDLE_EINTR(dup(native_pixmap.GetDmaBufFd(0)));
  if (fd < 0)
    return 0;

  uint32_t pixmap_id = xcb_generate_id(connection);
  xcb_dri3_pixmap_from_buffer(connection, pixmap_id, DefaultRootWindow(display),
                              native_pixmap.GetDmaBufPlaneSize(0),
                              native_pixmap.GetBufferSize().width(),
                              native_pixmap.GetBufferSize().height(),
                              native_pixmap.GetDmaBufPitch(0), depth, bpp, fd);
  return pixmap_id;
}

}  // namespace

GLImageGLXNativePixmap::GLImageGLXNativePixmap(const gfx::Size& size,
                                               gfx::BufferFormat format)
    : GLImageGLX(size, format) {}

GLImageGLXNativePixmap::~GLImageGLXNativePixmap() = default;

bool GLImageGLXNativePixmap::Initialize(
    scoped_refptr<gfx::NativePixmap> pixmap) {
  native_pixmap_ = pixmap;

  return GLImageGLX::Initialize(XPixmapFromNativePixmap(
      *static_cast<gfx::NativePixmapDmaBuf*>(native_pixmap_.get()),
      Depth(format()), Bpp(format())));
}

}  // namespace gl
