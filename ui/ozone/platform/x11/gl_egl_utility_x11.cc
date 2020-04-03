// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/gl_egl_utility_x11.h"

#include "ui/base/x/x11_gl_egl_utility.h"

namespace ui {

GLEGLUtilityX11::GLEGLUtilityX11() = default;
GLEGLUtilityX11::~GLEGLUtilityX11() = default;

void GLEGLUtilityX11::GetAdditionalEGLAttributes(
    EGLenum platform_type,
    std::vector<EGLAttrib>* display_attributes) {
  GetPlatformExtraDisplayAttribs(platform_type, display_attributes);
}

void GLEGLUtilityX11::ChooseEGLAlphaAndBufferSize(EGLint* alpha_size,
                                                  EGLint* buffer_size) {
  ChoosePlatformCustomAlphaAndBufferSize(alpha_size, buffer_size);
}

}  // namespace ui
