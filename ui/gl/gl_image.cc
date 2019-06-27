// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image.h"

#if defined(OS_ANDROID)
#include "base/android/scoped_hardware_buffer_fence_sync.h"
#endif

namespace gl {

bool GLImage::BindTexImageWithInternalformat(unsigned target,
                                             unsigned internalformat) {
  return false;
}

void GLImage::SetColorSpace(const gfx::ColorSpace& color_space) {
  color_space_ = color_space;
}

bool GLImage::EmulatingRGB() const {
  return false;
}

GLImage::Type GLImage::GetType() const {
  return Type::NONE;
}

#if defined(OS_ANDROID)
std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
GLImage::GetAHardwareBuffer() {
  return nullptr;
}
#endif

bool GLImage::HasMutableState() const {
  return true;
}

}  // namespace gl
