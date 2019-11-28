// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/cast_media_session_controller.h"

#include "base/time/time.h"
#include "chrome/common/media_router/mojom/media_status.mojom.h"
#include "services/media_session/public/mojom/constants.mojom.h"

namespace {

constexpr base::TimeDelta kDefaultSeekTimeSeconds =
    base::TimeDelta::FromSeconds(media_session::mojom::kDefaultSeekTimeSeconds);

}  // namespace

CastMediaSessionController::CastMediaSessionController(
    mojo::Remote<media_router::mojom::MediaController> route_controller)
    : route_controller_(std::move(route_controller)) {}

CastMediaSessionController::~CastMediaSessionController() {}

void CastMediaSessionController::Send(
    media_session::mojom::MediaSessionAction action) {
  if (!media_status_)
    return;

  switch (action) {
    case media_session::mojom::MediaSessionAction::kPlay:
      route_controller_->Play();
      return;
    case media_session::mojom::MediaSessionAction::kPause:
      route_controller_->Pause();
      return;
    case media_session::mojom::MediaSessionAction::kPreviousTrack:
      route_controller_->PreviousTrack();
      return;
    case media_session::mojom::MediaSessionAction::kNextTrack:
      route_controller_->NextTrack();
      return;
    case media_session::mojom::MediaSessionAction::kSeekBackward:
      route_controller_->Seek(PutWithinBounds(media_status_->current_time -
                                              kDefaultSeekTimeSeconds));
      return;
    case media_session::mojom::MediaSessionAction::kSeekForward:
      route_controller_->Seek(PutWithinBounds(media_status_->current_time +
                                              kDefaultSeekTimeSeconds));
      return;
    case media_session::mojom::MediaSessionAction::kStop:
      route_controller_->Pause();
      return;
    case media_session::mojom::MediaSessionAction::kSkipAd:
    case media_session::mojom::MediaSessionAction::kSeekTo:
    case media_session::mojom::MediaSessionAction::kScrubTo:
      NOTREACHED();
      return;
  }
}

void CastMediaSessionController::OnMediaStatusUpdated(
    media_router::mojom::MediaStatusPtr media_status) {
  media_status_ = std::move(media_status);
}

void CastMediaSessionController::FlushForTesting() {
  route_controller_.FlushForTesting();
}

base::TimeDelta CastMediaSessionController::PutWithinBounds(
    const base::TimeDelta& time) {
  if (time < base::TimeDelta() || !media_status_)
    return base::TimeDelta();
  if (time > media_status_->duration)
    return media_status_->duration;
  return time;
}
