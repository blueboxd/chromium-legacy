// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_WEBGL_RENDERING_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_WEBGL_RENDERING_CONTEXT_H_

#if defined(SUPPORT_WEBGL2_COMPUTE_CONTEXT)
#include "third_party/blink/renderer/bindings/modules/v8/webgl_rendering_context_or_webgl2_rendering_context_or_webgl2_compute_rendering_context.h"
#else
#include "third_party/blink/renderer/bindings/modules/v8/webgl_rendering_context_or_webgl2_rendering_context.h"
#endif

namespace blink {

#if defined(SUPPORT_WEBGL2_COMPUTE_CONTEXT)
using XRWebGLRenderingContext =
    WebGLRenderingContextOrWebGL2RenderingContextOrWebGL2ComputeRenderingContext;
#else
using XRWebGLRenderingContext = WebGLRenderingContextOrWebGL2RenderingContext;
#endif

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_WEBGL_RENDERING_CONTEXT_H_
