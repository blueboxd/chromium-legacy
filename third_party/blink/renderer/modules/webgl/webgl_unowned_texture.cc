// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_unowned_texture.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

WebGLUnownedTexture::WebGLUnownedTexture(WebGLRenderingContextBase* ctx,
                                         GLuint texture,
                                         GLenum target)
    : WebGLTexture(ctx, texture, target) {}

void WebGLUnownedTexture::DeleteObjectImpl(gpu::gles2::GLES2Interface* gl) {
  object_ = 0;
}

WebGLUnownedTexture::~WebGLUnownedTexture() = default;

}  // namespace blink
