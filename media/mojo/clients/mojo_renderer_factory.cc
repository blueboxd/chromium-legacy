// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_renderer_factory.h"

#include <utility>

#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "media/mojo/clients/mojo_renderer.h"
#include "media/mojo/mojom/renderer_extensions.mojom.h"
#include "media/renderers/decrypting_renderer.h"
#include "media/renderers/video_overlay_factory.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/connect.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace media {

MojoRendererFactory::MojoRendererFactory(
    media::mojom::InterfaceFactory* interface_factory)
    : interface_factory_(interface_factory) {
  DCHECK(interface_factory_);
}

MojoRendererFactory::~MojoRendererFactory() = default;

std::unique_ptr<Renderer> MojoRendererFactory::CreateRenderer(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& /* worker_task_runner */,
    AudioRendererSink* /* audio_renderer_sink */,
    VideoRendererSink* video_renderer_sink,
    const RequestOverlayInfoCB& /* request_overlay_info_cb */,
    const gfx::ColorSpace& /* target_color_space */) {
  DCHECK(interface_factory_);

  auto overlay_factory = std::make_unique<VideoOverlayFactory>();

  mojom::RendererPtr renderer_ptr;
  interface_factory_->CreateDefaultRenderer(std::string(),
                                            mojo::MakeRequest(&renderer_ptr));

  return std::make_unique<MojoRenderer>(
      media_task_runner, std::move(overlay_factory), video_renderer_sink,
      std::move(renderer_ptr));
}

#if BUILDFLAG(ENABLE_CAST_RENDERER)
std::unique_ptr<MojoRenderer> MojoRendererFactory::CreateCastRenderer(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    VideoRendererSink* video_renderer_sink) {
  DCHECK(interface_factory_);

  auto overlay_factory = std::make_unique<VideoOverlayFactory>();

  mojom::RendererPtr renderer_ptr;
  interface_factory_->CreateCastRenderer(overlay_factory->overlay_plane_id(),
                                         mojo::MakeRequest(&renderer_ptr));

  return std::make_unique<MojoRenderer>(
      media_task_runner, std::move(overlay_factory), video_renderer_sink,
      std::move(renderer_ptr));
}
#endif  // BUILDFLAG(ENABLE_CAST_RENDERER)

#if defined(OS_ANDROID)
std::unique_ptr<MojoRenderer> MojoRendererFactory::CreateFlingingRenderer(
    const std::string& presentation_id,
    mojom::FlingingRendererClientExtensionPtr client_extension_ptr,
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    VideoRendererSink* video_renderer_sink) {
  DCHECK(interface_factory_);
  mojom::RendererPtr renderer_ptr;

  interface_factory_->CreateFlingingRenderer(presentation_id,
                                             std::move(client_extension_ptr),
                                             mojo::MakeRequest(&renderer_ptr));

  return std::make_unique<MojoRenderer>(
      media_task_runner, nullptr, video_renderer_sink, std::move(renderer_ptr));
}

std::unique_ptr<MojoRenderer> MojoRendererFactory::CreateMediaPlayerRenderer(
    mojom::MediaPlayerRendererExtensionRequest renderer_extension_request,
    mojom::MediaPlayerRendererClientExtensionPtr client_extension_ptr,
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    VideoRendererSink* video_renderer_sink) {
  DCHECK(interface_factory_);
  mojom::RendererPtr renderer_ptr;

  interface_factory_->CreateMediaPlayerRenderer(
      std::move(client_extension_ptr), mojo::MakeRequest(&renderer_ptr),
      std::move(renderer_extension_request));

  return std::make_unique<MojoRenderer>(
      media_task_runner, nullptr, video_renderer_sink, std::move(renderer_ptr));
}
#endif  // defined(OS_ANDROID)

}  // namespace media
