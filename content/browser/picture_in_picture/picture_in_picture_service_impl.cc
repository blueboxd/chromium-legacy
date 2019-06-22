// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/picture_in_picture/picture_in_picture_service_impl.h"

#include <utility>

#include "content/browser/picture_in_picture/picture_in_picture_session.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents_delegate.h"

namespace content {

// static
void PictureInPictureServiceImpl::Create(
    RenderFrameHost* render_frame_host,
    blink::mojom::PictureInPictureServiceRequest request) {
  DCHECK(render_frame_host);
  new PictureInPictureServiceImpl(render_frame_host, std::move(request));
}

// static
PictureInPictureServiceImpl* PictureInPictureServiceImpl::CreateForTesting(
    RenderFrameHost* render_frame_host,
    blink::mojom::PictureInPictureServiceRequest request) {
  return new PictureInPictureServiceImpl(render_frame_host, std::move(request));
}

void PictureInPictureServiceImpl::StartSession(
    uint32_t player_id,
    const base::Optional<viz::SurfaceId>& surface_id,
    const gfx::Size& natural_size,
    bool show_play_pause_button,
    bool show_mute_button,
    blink::mojom::PictureInPictureSessionObserverPtr observer,
    StartSessionCallback callback) {
  blink::mojom::PictureInPictureSessionPtr session_ptr;
  gfx::Size window_size;

  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents());

  auto result = web_contents_impl->EnterPictureInPicture(surface_id.value(),
                                                         natural_size);

  // Picture-in-Picture may not be supported by all embedders, so we should only
  // create the session if the EnterPictureInPicture request was successful.
  if (result == PictureInPictureResult::kSuccess) {
    active_session_ = std::make_unique<PictureInPictureSession>(
        this, MediaPlayerId(render_frame_host_, player_id), surface_id,
        natural_size, show_play_pause_button, show_mute_button,
        mojo::MakeRequest(&session_ptr), std::move(observer), &window_size);
  }

  std::move(callback).Run(std::move(session_ptr), window_size);
}

PictureInPictureServiceImpl::PictureInPictureServiceImpl(
    RenderFrameHost* render_frame_host,
    blink::mojom::PictureInPictureServiceRequest request)
    : FrameServiceBase(render_frame_host, std::move(request)),
      render_frame_host_(render_frame_host) {}

PictureInPictureServiceImpl::~PictureInPictureServiceImpl() {
  // If the service is destroyed because the frame was destroyed, the session
  // may still be active and it has to be shutdown before its dtor runs.
  if (active_session_)
    active_session_->Shutdown();
}

}  // namespace content
