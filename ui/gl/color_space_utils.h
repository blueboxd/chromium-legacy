// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_COLOR_SPACE_UTILS_H_
#define UI_GL_COLOR_SPACE_UTILS_H_

#include "build/build_config.h"
#include "ui/gl/gl_export.h"

typedef unsigned int GLenum;

namespace gfx {
class ColorSpace;
}  // namespace gfx

namespace gl {

class GL_EXPORT ColorSpaceUtils {
 public:
  // Get the color space enum value used for ResizeCHROMIUM.
  static GLenum GetGLColorSpace(const gfx::ColorSpace& color_space);

  // Get the color space used for GLSurface::Resize().
  static gfx::ColorSpace GetColorSpace(GLenum color_space);
};

}  // namespace gl

#endif  // UI_GL_COLOR_SPACE_UTILS_H_
