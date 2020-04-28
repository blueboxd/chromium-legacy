// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_X11_UTIL_H_
#define UI_GFX_X_X11_UTIL_H_

#include <stdint.h>

#include <memory>

#include "ui/gfx/gfx_export.h"

typedef unsigned long XAtom;
typedef unsigned long XID;
typedef unsigned long VisualID;
typedef struct _XcursorImage XcursorImage;
typedef union _XEvent XEvent;
typedef struct _XImage XImage;
typedef struct _XGC* GC;
typedef struct _XDisplay XDisplay;
typedef struct _XRegion XRegion;
typedef struct __GLXFBConfigRec* GLXFBConfig;
typedef XID GLXWindow;
typedef XID GLXDrawable;

extern "C" {
int XFree(void*);
}

namespace gfx {

template <class T, class R, R (*F)(T*)>
struct XObjectDeleter {
  inline void operator()(void* ptr) const { F(static_cast<T*>(ptr)); }
};

template <class T, class D = XObjectDeleter<void, int, XFree>>
using XScopedPtr = std::unique_ptr<T, D>;

// Get the XDisplay singleton.  Prefer x11::Connection::Get() instead.
GFX_EXPORT XDisplay* GetXDisplay();

// Given a connection to an X server, opens a new parallel connection to the
// same X server.  It's the caller's responsibility to call XCloseDisplay().
GFX_EXPORT XDisplay* CloneXDisplay(XDisplay* display);

// Return the number of bits-per-pixel for a pixmap of the given depth
GFX_EXPORT int BitsPerPixelForPixmapDepth(XDisplay* display, int depth);

// Draws ARGB data on the given pixmap using the given GC, converting to the
// server side visual depth as needed.  Destination is assumed to be the same
// dimensions as |data| or larger.  |data| is also assumed to be in row order
// with each line being exactly |width| * 4 bytes long.
GFX_EXPORT void PutARGBImage(XDisplay* display,
                             void* visual,
                             int depth,
                             XID pixmap,
                             void* pixmap_gc,
                             const uint8_t* data,
                             int width,
                             int height);

// Same as above only more general:
// - |data_width| and |data_height| refer to the data image
// - |src_x|, |src_y|, |copy_width| and |copy_height| define source region
// - |dst_x|, |dst_y|, |copy_width| and |copy_height| define destination region
GFX_EXPORT void PutARGBImage(XDisplay* display,
                             void* visual,
                             int depth,
                             XID pixmap,
                             void* pixmap_gc,
                             const uint8_t* data,
                             int data_width,
                             int data_height,
                             int src_x,
                             int src_y,
                             int dst_x,
                             int dst_y,
                             int copy_width,
                             int copy_height);

}  // namespace gfx

#endif  // UI_GFX_X_X11_UTIL_H_
