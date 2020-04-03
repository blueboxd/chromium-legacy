// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/init/gl_display_egl_util_x11.h"

#include "ui/base/x/x11_gl_egl_utility.h"

namespace gl {

// static
GLDisplayEglUtilX11* GLDisplayEglUtilX11::GetInstance() {
  static base::NoDestructor<GLDisplayEglUtilX11> instance;
  return instance.get();
}

void GLDisplayEglUtilX11::GetPlatformExtraDisplayAttribs(
    EGLenum platform_type,
    std::vector<EGLAttrib>* attributes) {
  ui::GetPlatformExtraDisplayAttribs(platform_type, attributes);
}

void GLDisplayEglUtilX11::ChoosePlatformCustomAlphaAndBufferSize(
    EGLint* alpha_size,
    EGLint* buffer_size) {
  ui::ChoosePlatformCustomAlphaAndBufferSize(alpha_size, buffer_size);
}

GLDisplayEglUtilX11::GLDisplayEglUtilX11() = default;

GLDisplayEglUtilX11::~GLDisplayEglUtilX11() = default;

}  // namespace gl