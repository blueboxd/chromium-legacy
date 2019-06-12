// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/picture_in_picture/picture_in_picture_session.h"

#include "content/browser/picture_in_picture/picture_in_picture_service_impl.h"
#include "content/browser/picture_in_picture/picture_in_picture_window_controller_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents_delegate.h"

namespace content {

PictureInPictureSession::PictureInPictureSession(
    PictureInPictureServiceImpl* service,
    const MediaPlayerId& player_id,
    const base::Optional<viz::SurfaceId>& surface_id,
    const gfx::Size& natural_size,
    bool show_play_pause_button,
    bool show_mute_button,
    mojo::InterfaceRequest<blink::mojom::PictureInPictureSession> request,
    blink::mojom::PictureInPictureSessionObserverPtr observer,
    gfx::Size* window_size)
    : service_(service),
      binding_(this, std::move(request)),
      player_id_(player_id),
      observer_(std::move(observer)) {
  binding_.set_connection_error_handler(base::BindOnce(
      &PictureInPictureSession::OnConnectionError, base::Unretained(this)));

  // TODO(mlamouri): figure out why this can be null and have the method return
  // a const ref.
  auto* controller = GetController();
  if (controller)
    controller->SetActiveSession(this);

  // TODO(beccahughes): Pass PictureInPictureResult::kNotSupported back to
  // Blink.
  auto result = GetWebContentsImpl()->EnterPictureInPicture(surface_id.value(),
                                                            natural_size);
  DCHECK_EQ(PictureInPictureResult::kSuccess, result);

  if (controller) {
    controller->SetAlwaysHidePlayPauseButton(show_play_pause_button);
    controller->SetAlwaysHideMuteButton(show_mute_button);
    *window_size = controller->GetSize();
  }
}

PictureInPictureSession::~PictureInPictureSession() {
  DCHECK(is_stopping_);
}

void PictureInPictureSession::Stop(StopCallback callback) {
  StopInternal(std::move(callback));
}

void PictureInPictureSession::Update(
    uint32_t player_id,
    const base::Optional<viz::SurfaceId>& surface_id,
    const gfx::Size& natural_size,
    bool show_play_pause_button,
    bool show_mute_button) {
  player_id_ = MediaPlayerId(service_->render_frame_host_, player_id);

  // The PictureInPictureWindowController instance may not have been created by
  // the embedder.
  if (auto* pip_controller = GetController()) {
    pip_controller->EmbedSurface(surface_id.value(), natural_size);
    pip_controller->SetAlwaysHidePlayPauseButton(show_play_pause_button);
    pip_controller->SetAlwaysHideMuteButton(show_mute_button);
    pip_controller->SetActiveSession(this);
  }
}

void PictureInPictureSession::NotifyWindowResized(const gfx::Size& size) {
  observer_->OnWindowSizeChanged(size);
}

void PictureInPictureSession::Shutdown() {
  if (is_stopping_)
    return;

  StopInternal(base::NullCallback());
}

void PictureInPictureSession::StopInternal(StopCallback callback) {
  DCHECK(!is_stopping_);

  is_stopping_ = true;

  GetWebContentsImpl()->ExitPictureInPicture();

  // `OnStopped()` should only be called if there is no callback to run, as a
  // contract in the API.
  if (callback)
    std::move(callback).Run();
  else
    observer_->OnStopped();

  if (auto* controller = GetController())
    controller->SetActiveSession(nullptr);

  // Reset must happen after everything is done as it will destroy |this|.
  service_->active_session_.reset();
}

void PictureInPictureSession::OnConnectionError() {
  // StopInternal() will self destruct which will close the bindings.
  StopInternal(base::NullCallback());
}

WebContentsImpl* PictureInPictureSession::GetWebContentsImpl() {
  return static_cast<WebContentsImpl*>(service_->web_contents());
}

PictureInPictureWindowControllerImpl* PictureInPictureSession::GetController() {
  return PictureInPictureWindowControllerImpl::GetOrCreateForWebContents(
      GetWebContentsImpl());
}

}  // namespace content
