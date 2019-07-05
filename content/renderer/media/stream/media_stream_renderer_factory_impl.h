// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_STREAM_MEDIA_STREAM_RENDERER_FACTORY_IMPL_H_
#define CONTENT_RENDERER_MEDIA_STREAM_MEDIA_STREAM_RENDERER_FACTORY_IMPL_H_

#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_renderer_factory.h"

#include <string>

#include "base/macros.h"

namespace content {

class MediaStreamRendererFactoryImpl
    : public blink::WebMediaStreamRendererFactory {
 public:
  MediaStreamRendererFactoryImpl();
  ~MediaStreamRendererFactoryImpl() override;

  scoped_refptr<blink::WebMediaStreamVideoRenderer> GetVideoRenderer(
      const blink::WebMediaStream& web_stream,
      const blink::WebMediaStreamVideoRenderer::RepaintCB& repaint_cb,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> main_render_task_runner)
      override;

  scoped_refptr<blink::WebMediaStreamAudioRenderer> GetAudioRenderer(
      const blink::WebMediaStream& web_stream,
      blink::WebLocalFrame* web_frame,
      const std::string& device_id) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaStreamRendererFactoryImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_STREAM_MEDIA_STREAM_RENDERER_FACTORY_IMPL_H_
