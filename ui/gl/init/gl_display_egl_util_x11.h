// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_INIT_GL_DISPLAY_EGL_UTIL_X11_H_
#define UI_GL_INIT_GL_DISPLAY_EGL_UTIL_X11_H_

#include <vector>

#include "base/no_destructor.h"
#include "ui/gl/gl_display_egl_util.h"

namespace gl {

class GLDisplayEglUtilX11 : public GLDisplayEglUtil {
 public:
  static GLDisplayEglUtilX11* GetInstance();

  // GLDisplayEglUtil overrides:
  void GetPlatformExtraDisplayAttribs(
      EGLenum platform_type,
      std::vector<EGLAttrib>* attributes) override;
  void ChoosePlatformCustomAlphaAndBufferSize(EGLint* alpha_size,
                                              EGLint* buffer_size) override;

 private:
  friend base::NoDestructor<GLDisplayEglUtilX11>;
  GLDisplayEglUtilX11();
  ~GLDisplayEglUtilX11() override;
  GLDisplayEglUtilX11(const GLDisplayEglUtilX11& util) = delete;
  GLDisplayEglUtilX11& operator=(const GLDisplayEglUtilX11& util) = delete;
};

}  // namespace gl

#endif  // UI_GL_INIT_GL_DISPLAY_EGL_UTIL_X11_H_
