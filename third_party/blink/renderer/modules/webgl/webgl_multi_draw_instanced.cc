// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_multi_draw_instanced.h"

#include "gpu/command_buffer/client/gles2_interface.h"

namespace blink {

WebGLMultiDrawInstanced::WebGLMultiDrawInstanced(
    WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled(
      "GL_WEBGL_multi_draw_instanced");
  context->ExtensionsUtil()->EnsureExtensionEnabled("GL_ANGLE_multi_draw");
}

WebGLMultiDrawInstanced* WebGLMultiDrawInstanced::Create(
    WebGLRenderingContextBase* context) {
  return MakeGarbageCollected<WebGLMultiDrawInstanced>(context);
}

WebGLExtensionName WebGLMultiDrawInstanced::GetName() const {
  return kWebGLMultiDrawInstancedName;
}

bool WebGLMultiDrawInstanced::Supported(WebGLRenderingContextBase* context) {
  return context->ExtensionsUtil()->SupportsExtension(
             "GL_WEBGL_multi_draw_instanced") ||
         (context->ExtensionsUtil()->SupportsExtension("GL_ANGLE_multi_draw") &&
          context->ExtensionsUtil()->SupportsExtension(
              "GL_ANGLE_instanced_arrays"));
}

const char* WebGLMultiDrawInstanced::ExtensionName() {
  return "WEBGL_multi_draw_instanced";
}

void WebGLMultiDrawInstanced::multiDrawArraysInstancedImpl(
    GLenum mode,
    const base::span<const int32_t>& firsts,
    GLuint firstsOffset,
    const base::span<const int32_t>& counts,
    GLuint countsOffset,
    const base::span<const int32_t>& instanceCounts,
    GLuint instanceCountsOffset,
    GLsizei drawcount) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost() ||
      !ValidateDrawcount(&scoped, "glMultiDrawArraysWEBGL", drawcount) ||
      !ValidateArray(&scoped, "glMultiDrawArraysWEBGL",
                     "firstsOffset out of bounds", firsts.size(), firstsOffset,
                     drawcount) ||
      !ValidateArray(&scoped, "glMultiDrawArraysWEBGL",
                     "countsOffset out of bounds", counts.size(), countsOffset,
                     drawcount) ||
      !ValidateArray(&scoped, "glMultiDrawElementsWEBGL",
                     "instanceCountsOffset out of bounds",
                     instanceCounts.size(), instanceCountsOffset, drawcount)) {
    return;
  }

  scoped.Context()->ContextGL()->MultiDrawArraysInstancedWEBGL(
      mode, &firsts[firstsOffset], &counts[countsOffset],
      &instanceCounts[instanceCountsOffset], drawcount);
}

void WebGLMultiDrawInstanced::multiDrawElementsInstancedImpl(
    GLenum mode,
    const base::span<const int32_t>& counts,
    GLuint countsOffset,
    GLenum type,
    const base::span<const int32_t>& offsets,
    GLuint offsetsOffset,
    const base::span<const int32_t>& instanceCounts,
    GLuint instanceCountsOffset,
    GLsizei drawcount) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost() ||
      !ValidateDrawcount(&scoped, "glMultiDrawElementsWEBGL", drawcount) ||
      !ValidateArray(&scoped, "glMultiDrawElementsWEBGL",
                     "countsOffset out of bounds", counts.size(), countsOffset,
                     drawcount) ||
      !ValidateArray(&scoped, "glMultiDrawElementsWEBGL",
                     "offsetsOffset out of bounds", offsets.size(),
                     offsetsOffset, drawcount) ||
      !ValidateArray(&scoped, "glMultiDrawElementsWEBGL",
                     "instanceCountsOffset out of bounds",
                     instanceCounts.size(), instanceCountsOffset, drawcount)) {
    return;
  }

  scoped.Context()->ContextGL()->MultiDrawElementsInstancedWEBGL(
      mode, &counts[countsOffset], type, &offsets[offsetsOffset],
      &instanceCounts[instanceCountsOffset], drawcount);
}

}  // namespace blink
