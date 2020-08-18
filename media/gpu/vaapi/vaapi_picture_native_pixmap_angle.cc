// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_picture_native_pixmap_angle.h"

#include "base/file_descriptor_posix.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "media/gpu/vaapi/va_surface.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "ui/gfx/linux/gbm_buffer.h"
#include "ui/gfx/linux/gpu_memory_buffer_support_x11.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_image_native_pixmap.h"
#include "ui/gl/scoped_binders.h"

namespace media {

VaapiPictureNativePixmapAngle::VaapiPictureNativePixmapAngle(
    scoped_refptr<VaapiWrapper> vaapi_wrapper,
    const MakeGLContextCurrentCallback& make_context_current_cb,
    const BindGLImageCallback& bind_image_cb,
    int32_t picture_buffer_id,
    const gfx::Size& visible_size,
    const gfx::Size& size,
    uint32_t service_texture_id,
    uint32_t client_texture_id,
    uint32_t texture_target)
    : VaapiPictureNativePixmap(std::move(vaapi_wrapper),
                               make_context_current_cb,
                               bind_image_cb,
                               picture_buffer_id,
                               size,
                               visible_size,
                               service_texture_id,
                               client_texture_id,
                               texture_target) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Check that they're either both 0 or both not 0 (tests will set both to 0)
  DCHECK(!!service_texture_id == !!client_texture_id);
}

VaapiPictureNativePixmapAngle::~VaapiPictureNativePixmapAngle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (gl_image_ && make_context_current_cb_.Run()) {
    gl_image_->ReleaseTexImage(texture_target_);
    DCHECK_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR));
  }
}

Status VaapiPictureNativePixmapAngle::Allocate(gfx::BufferFormat format) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return StatusCode::kGenericErrorPleaseRemove;
}

bool VaapiPictureNativePixmapAngle::ImportGpuMemoryBufferHandle(
    gfx::BufferFormat format,
    gfx::GpuMemoryBufferHandle gpu_memory_buffer_handlee) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return false;
}

}  // namespace media
