// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/win/media_foundation_renderer_factory.h"

#include "base/single_thread_task_runner.h"
#include "media/renderers/win/media_foundation_renderer.h"

namespace media {

MediaFoundationRendererFactory::MediaFoundationRendererFactory() = default;

MediaFoundationRendererFactory::~MediaFoundationRendererFactory() = default;

// MediaFoundationRenderer does most audio/video rendering by itself and doesn't
// use most of the parameters passed in.
std::unique_ptr<Renderer> MediaFoundationRendererFactory::CreateRenderer(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& /*worker_task_runner*/,
    AudioRendererSink* /*audio_renderer_sink*/,
    VideoRendererSink* /*video_renderer_sink*/,
    RequestOverlayInfoCB /*request_overlay_info_cb*/,
    const gfx::ColorSpace& /*target_color_space*/) {
  // TODO(xhwang): Investigate how to set |muted| and update after composition
  // is connected.
  auto renderer = std::make_unique<MediaFoundationRenderer>(
      /*muted=*/false, media_task_runner,
      /*force_dcomp_mode_for_testing=*/true);

  return renderer;
}

}  // namespace media