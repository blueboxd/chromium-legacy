// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_UTIL_INTERNAL_H_
#define UI_BASE_X_X11_UTIL_INTERNAL_H_

// This file declares utility functions for X11 (Linux only).

#include <memory>
#include <unordered_map>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_types.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace ui {

// --------------------------------------------------------------------------
// NOTE: these functions cache the results and must be called from the UI
// thread.
// Get the XRENDER format id for ARGB32 (Skia's format).
//
// NOTE:Currently this don't support multiple screens/displays.
COMPONENT_EXPORT(UI_BASE_X)
XRenderPictFormat* GetRenderARGB32Format(Display* dpy);

// --------------------------------------------------------------------------
// X11 error handling.
// Sets the X Error Handlers. Passing NULL for either will enable the default
// error handler, which if called will log the error and abort the process.
COMPONENT_EXPORT(UI_BASE_X)
void SetX11ErrorHandlers(XErrorHandler error_handler,
                         XIOErrorHandler io_error_handler);

// NOTE: This function should not be called directly from the
// X11 Error handler because it queries the server to decode the
// error message, which may trigger other errors. A suitable workaround
// is to post a task in the error handler to call this function.
COMPONENT_EXPORT(UI_BASE_X)
void LogErrorEventDescription(Display* dpy, const XErrorEvent& error_event);

// --------------------------------------------------------------------------
// Selects a visual with a preference for alpha support on compositing window
// managers.
class COMPONENT_EXPORT(UI_BASE_X) XVisualManager {
 public:
  static XVisualManager* GetInstance();

  // Picks the best argb or opaque visual given |want_argb_visual|.
  void ChooseVisualForWindow(bool want_argb_visual,
                             x11::VisualId* visual_id,
                             uint8_t* depth,
                             bool* visual_has_alpha);

  bool GetVisualInfo(x11::VisualId visual_id,
                     uint8_t* depth,
                     bool* visual_has_alpha);

  // Called by GpuDataManagerImplPrivate when GPUInfo becomes available.  It is
  // necessary for the GPU process to find out which visuals are best for GL
  // because we don't want to load GL in the browser process.  Returns false iff
  // |default_visual_id| or |transparent_visual_id| are invalid.
  bool OnGPUInfoChanged(bool software_rendering,
                        x11::VisualId default_visual_id,
                        x11::VisualId transparent_visual_id);

  // Are all of the system requirements met for using transparent visuals?
  bool ArgbVisualAvailable() const;

  ~XVisualManager();

 private:
  friend struct base::DefaultSingletonTraits<XVisualManager>;

  class XVisualData {
   public:
    XVisualData(uint8_t depth, const x11::VisualType* info);
    ~XVisualData();

    uint8_t depth = 0;
    const x11::VisualType* info = nullptr;
  };

  XVisualManager();

  bool GetVisualInfoImpl(x11::VisualId visual_id,
                         uint8_t* depth,
                         bool* visual_has_alpha);

  mutable base::Lock lock_;

  std::unordered_map<x11::VisualId, std::unique_ptr<XVisualData>> visuals_;

  x11::Connection* const connection_;

  x11::VisualId default_visual_id_{};

  // The system visual is usually the same as the default visual, but
  // may not be in general.
  x11::VisualId system_visual_id_{};
  x11::VisualId transparent_visual_id_{};

  bool using_software_rendering_ = false;
  bool have_gpu_argb_visual_ = false;

  DISALLOW_COPY_AND_ASSIGN(XVisualManager);
};

}  // namespace ui

#endif  // UI_BASE_X_X11_UTIL_INTERNAL_H_
