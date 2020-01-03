// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/color_space_utils.h"

#include "ui/gfx/color_space.h"
#include "ui/gl/gl_bindings.h"

namespace gl {

// static
GLenum ColorSpaceUtils::GetGLColorSpace(const gfx::ColorSpace& color_space) {
  if (color_space == gfx::ColorSpace::CreateSCRGBLinear())
    return GL_COLOR_SPACE_SCRGB_LINEAR_CHROMIUM;

  if (color_space == gfx::ColorSpace::CreateHDR10())
    return GL_COLOR_SPACE_HDR10_CHROMIUM;

  if (color_space == gfx::ColorSpace::CreateSRGB())
    return GL_COLOR_SPACE_SRGB_CHROMIUM;

  if (color_space == gfx::ColorSpace::CreateDisplayP3D65())
    return GL_COLOR_SPACE_DISPLAY_P3_CHROMIUM;

  return GL_COLOR_SPACE_UNSPECIFIED_CHROMIUM;
}

// static
gfx::ColorSpace ColorSpaceUtils::GetColorSpace(GLenum color_space) {
  switch (color_space) {
    case GL_COLOR_SPACE_UNSPECIFIED_CHROMIUM:
      break;
    case GL_COLOR_SPACE_SCRGB_LINEAR_CHROMIUM:
      return gfx::ColorSpace::CreateSCRGBLinear();
    case GL_COLOR_SPACE_HDR10_CHROMIUM:
      return gfx::ColorSpace::CreateHDR10();
    case GL_COLOR_SPACE_SRGB_CHROMIUM:
      return gfx::ColorSpace::CreateSRGB();
    case GL_COLOR_SPACE_DISPLAY_P3_CHROMIUM:
      return gfx::ColorSpace::CreateDisplayP3D65();
    default:
      DLOG(ERROR) << "Invalid color space.";
      break;
  }
  return gfx::ColorSpace();
}

}  // namespace gl
