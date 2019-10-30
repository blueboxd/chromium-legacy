// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_multi_draw.h"

#include "gpu/command_buffer/client/gles2_interface.h"

namespace blink {

WebGLMultiDraw::WebGLMultiDraw(WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled("GL_WEBGL_multi_draw");
  context->ExtensionsUtil()->EnsureExtensionEnabled("GL_ANGLE_multi_draw");
}

WebGLMultiDraw* WebGLMultiDraw::Create(WebGLRenderingContextBase* context) {
  return MakeGarbageCollected<WebGLMultiDraw>(context);
}

WebGLExtensionName WebGLMultiDraw::GetName() const {
  return kWebGLMultiDrawName;
}

bool WebGLMultiDraw::Supported(WebGLRenderingContextBase* context) {
  return context->ExtensionsUtil()->SupportsExtension("GL_WEBGL_multi_draw") ||
         context->ExtensionsUtil()->SupportsExtension("GL_ANGLE_multi_draw");
}

const char* WebGLMultiDraw::ExtensionName() {
  return "WEBGL_multi_draw";
}

void WebGLMultiDraw::multiDrawArraysImpl(
    GLenum mode,
    const base::span<const int32_t>& firsts,
    GLuint firstsOffset,
    const base::span<const int32_t>& counts,
    GLuint countsOffset,
    GLsizei drawcount) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost() ||
      !ValidateDrawcount(&scoped, "glMultiDrawArraysWEBGL", drawcount) ||
      !ValidateArray(&scoped, "glMultiDrawArraysWEBGL",
                     "firstsOffset out of bounds", firsts.size(), firstsOffset,
                     drawcount) ||
      !ValidateArray(&scoped, "glMultiDrawArraysWEBGL",
                     "countsOffset out of bounds", counts.size(), countsOffset,
                     drawcount)) {
    return;
  }

  scoped.Context()->ContextGL()->MultiDrawArraysWEBGL(
      mode, &firsts[firstsOffset], &counts[countsOffset], drawcount);
}

void WebGLMultiDraw::multiDrawElementsImpl(
    GLenum mode,
    const base::span<const int32_t>& counts,
    GLuint countsOffset,
    GLenum type,
    const base::span<const int32_t>& offsets,
    GLuint offsetsOffset,
    GLsizei drawcount) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost() ||
      !ValidateDrawcount(&scoped, "glMultiDrawElementsWEBGL", drawcount) ||
      !ValidateArray(&scoped, "glMultiDrawElementsWEBGL",
                     "countsOffset out of bounds", counts.size(), countsOffset,
                     drawcount) ||
      !ValidateArray(&scoped, "glMultiDrawElementsWEBGL",
                     "offsetsOffset out of bounds", offsets.size(),
                     offsetsOffset, drawcount)) {
    return;
  }

  scoped.Context()->ContextGL()->MultiDrawElementsWEBGL(
      mode, &counts[countsOffset], type, &offsets[offsetsOffset], drawcount);
}

}  // namespace blink
