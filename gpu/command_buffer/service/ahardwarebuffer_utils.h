// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_AHARDWAREBUFFER_UTILS_H_
#define GPU_COMMAND_BUFFER_SERVICE_AHARDWAREBUFFER_UTILS_H_

#include "components/viz/common/resources/resource_format.h"
#include "gpu/gpu_gles2_export.h"

extern "C" typedef struct AHardwareBuffer AHardwareBuffer;

typedef unsigned int GLenum;

namespace gfx {
class ColorSpace;
class Rect;
class Size;
}  // namespace gfx

namespace gpu {
namespace gles2 {
class Texture;
}  // namespace gles2

// TODO(vikassoni): In future we will need to expose the set of formats and
// constraints (e.g. max size) to the clients somehow that are available for
// certain combinations of SharedImageUsage flags (e.g. when Vulkan is on,
// SHARED_IMAGE_USAGE_GLES2 + SHARED_IMAGE_USAGE_DISPLAY implies AHB, so those
// restrictions apply, but that's decided on the service side).
// For now getting supported format is a static mechanism like this. We
// probably need something like gpu::Capabilities.texture_target_exception_list.

// Returns whether the format is supported by AHardwareBuffer.
bool GPU_GLES2_EXPORT
AHardwareBufferSupportedFormat(viz::ResourceFormat format);

// Returns the corresponding AHardwareBuffer format.
unsigned int GPU_GLES2_EXPORT AHardwareBufferFormat(viz::ResourceFormat format);

// Generates a gles2 texture from AHB. This method must be called with a current
// GLContext which will be used to create the Texture. This method adds a
// lightweight ref on the Texture which the caller is responsible for releasing.
gles2::Texture* GenGLTexture(AHardwareBuffer* buffer,
                             GLenum target,
                             const gfx::ColorSpace& color_space,
                             const gfx::Size& size,
                             const gfx::Rect& cleared_rect);

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_AHARDWAREBUFFER_UTILS_H_
