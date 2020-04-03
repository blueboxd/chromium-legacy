// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_PLATFORM_GL_EGL_UTILITY_H_
#define UI_OZONE_PUBLIC_PLATFORM_GL_EGL_UTILITY_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "base/component_export.h"
#include "third_party/khronos/EGL/egl.h"

namespace ui {

// Provides platform specific EGL attributes/configs.
class COMPONENT_EXPORT(OZONE_BASE) PlatformGLEGLUtility {
 public:
  virtual ~PlatformGLEGLUtility() = default;

  // Gets additional display attributes based on |platform_type|.
  virtual void GetAdditionalEGLAttributes(
      EGLenum platform_type,
      std::vector<EGLAttrib>* display_attributes) = 0;

  // Chooses alpha and buffer size values.
  virtual void ChooseEGLAlphaAndBufferSize(EGLint* alpha_size,
                                           EGLint* buffer_size) = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_PLATFORM_GL_EGL_UTILITY_H_
