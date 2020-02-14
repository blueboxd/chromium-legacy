// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_VIDEO_TEXTURE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_VIDEO_TEXTURE_H_

#include "third_party/blink/renderer/modules/webgl/webgl_extension.h"

namespace blink {

class HTMLVideoElement;
class VideoFrameMetadata;

class WebGLVideoTexture final : public WebGLExtension {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static bool Supported(WebGLRenderingContextBase*);
  static const char* ExtensionName();

  explicit WebGLVideoTexture(WebGLRenderingContextBase*);

  WebGLExtensionName GetName() const override;

  void Trace(Visitor*) override;

  // Get video frame from video frame compositor and bind it to platform
  // texture.
  VideoFrameMetadata* VideoElementTargetVideoTexture(ExecutionContext*,
                                                     unsigned,
                                                     HTMLVideoElement*,
                                                     ExceptionState&);

 private:
  Member<VideoFrameMetadata> current_frame_metadata_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_VIDEO_TEXTURE_H_
