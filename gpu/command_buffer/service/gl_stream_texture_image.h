// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GL_STREAM_TEXTURE_IMAGE_H_
#define GPU_COMMAND_BUFFER_SERVICE_GL_STREAM_TEXTURE_IMAGE_H_

#include "gpu/gpu_gles2_export.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gl/gl_image.h"

namespace gpu {
namespace gles2 {

// Specialization of GLImage that allows us to support (stream) textures
// that supply a texture matrix.
class GPU_GLES2_EXPORT GLStreamTextureImage : public gl::GLImage {
 public:
  // TODO(weiliangc): When Overlay is moved off command buffer and we use
  // SharedImage in all cases, this API should be deleted.
  virtual void NotifyPromotionHint(bool promotion_hint,
                                   int display_x,
                                   int display_y,
                                   int display_width,
                                   int display_height) = 0;

 protected:
  ~GLStreamTextureImage() override = default;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GL_STREAM_TEXTURE_IMAGE_H_
