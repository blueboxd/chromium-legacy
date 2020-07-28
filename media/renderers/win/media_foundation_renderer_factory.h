// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_RENDERER_FACTORY_H_
#define MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_RENDERER_FACTORY_H_

#include <stdint.h>

#include "media/base/media_export.h"
#include "media/base/renderer_factory.h"

namespace media {

// RendererFactory to create MediaFoundationRendererFactory.
class MEDIA_EXPORT MediaFoundationRendererFactory : public RendererFactory {
 public:
  MediaFoundationRendererFactory();
  MediaFoundationRendererFactory(const MediaFoundationRendererFactory&) =
      delete;
  MediaFoundationRendererFactory& operator=(
      const MediaFoundationRendererFactory&) = delete;
  ~MediaFoundationRendererFactory() final;

  // RendererFactory implementation.
  std::unique_ptr<Renderer> CreateRenderer(
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      const scoped_refptr<base::TaskRunner>& worker_task_runner,
      AudioRendererSink* audio_renderer_sink,
      VideoRendererSink* video_renderer_sink,
      RequestOverlayInfoCB request_overlay_info_cb,
      const gfx::ColorSpace& target_color_space) final;
};

}  // namespace media

#endif  // MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_RENDERER_FACTORY_H_
