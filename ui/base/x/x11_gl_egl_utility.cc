// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_gl_egl_utility.h"

#include "ui/base/x/x11_util_internal.h"
#include "ui/gfx/x/x11.h"
#include "ui/gl/gl_surface_egl.h"

#ifndef EGL_ANGLE_x11_visual
#define EGL_ANGLE_x11_visual 1
#define EGL_X11_VISUAL_ID_ANGLE 0x33A3
#endif /* EGL_ANGLE_x11_visual */

#ifndef EGL_ANGLE_platform_angle_null
#define EGL_ANGLE_platform_angle_null 1
#define EGL_PLATFORM_ANGLE_TYPE_NULL_ANGLE 0x33AE
#endif /* EGL_ANGLE_platform_angle_null */

namespace ui {

void GetPlatformExtraDisplayAttribs(EGLenum platform_type,
                                    std::vector<EGLAttrib>* attributes) {
  // ANGLE_NULL doesn't use the visual, and may run without X11 where we can't
  // get it anyway.
  if (platform_type != EGL_PLATFORM_ANGLE_TYPE_NULL_ANGLE) {
    Visual* visual;
    ui::XVisualManager::GetInstance()->ChooseVisualForWindow(
        true, &visual, nullptr, nullptr, nullptr);
    attributes->push_back(EGL_X11_VISUAL_ID_ANGLE);
    attributes->push_back(static_cast<EGLAttrib>(XVisualIDFromVisual(visual)));
  }
}

void ChoosePlatformCustomAlphaAndBufferSize(EGLint* alpha_size,
                                            EGLint* buffer_size) {
  // If we're using ANGLE_NULL, we may not have a display, in which case we
  // can't use XVisualManager.
  if (gl::GLSurfaceEGL::GetNativeDisplay() != EGL_DEFAULT_DISPLAY) {
    ui::XVisualManager::GetInstance()->ChooseVisualForWindow(
        true, nullptr, buffer_size, nullptr, nullptr);
    *alpha_size = *buffer_size == 32 ? 8 : 0;
  }
}

}  // namespace ui
