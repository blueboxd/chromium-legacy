// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/now_playing_info_center_notifier.h"

#include <memory>
#include <utility>

#include "services/media_session/public/mojom/constants.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/now_playing/now_playing_info_center_delegate.h"

namespace content {

NowPlayingInfoCenterNotifier::NowPlayingInfoCenterNotifier(
    service_manager::Connector* connector,
    std::unique_ptr<now_playing::NowPlayingInfoCenterDelegate>
        now_playing_info_center_delegate)
    : now_playing_info_center_delegate_(
          std::move(now_playing_info_center_delegate)) {
  // |connector| can be null in tests.
  if (!connector)
    return;

  // Connect to the MediaControllerManager and create a MediaController that
  // controls the active session so we can observe it.
  media_session::mojom::MediaControllerManagerPtr controller_manager_ptr;
  connector->BindInterface(media_session::mojom::kServiceName,
                           mojo::MakeRequest(&controller_manager_ptr));
  controller_manager_ptr->CreateActiveMediaController(
      mojo::MakeRequest(&media_controller_ptr_));

  // Observe the active media controller for changes to playback state and
  // supported actions.
  media_controller_ptr_->AddObserver(
      media_controller_observer_receiver_.BindNewPipeAndPassRemote());
}

NowPlayingInfoCenterNotifier::~NowPlayingInfoCenterNotifier() = default;

void NowPlayingInfoCenterNotifier::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr session_info) {
  session_info_ = std::move(session_info);
  if (session_info_) {
    if (session_info_->playback_state ==
        media_session::mojom::MediaPlaybackState::kPlaying) {
      now_playing_info_center_delegate_->SetPlaybackState(
          now_playing::NowPlayingInfoCenterDelegate::PlaybackState::kPlaying);
    } else {
      now_playing_info_center_delegate_->SetPlaybackState(
          now_playing::NowPlayingInfoCenterDelegate::PlaybackState::kPaused);
    }
  } else {
    now_playing_info_center_delegate_->SetPlaybackState(
        now_playing::NowPlayingInfoCenterDelegate::PlaybackState::kStopped);
  }
}

}  // namespace content
