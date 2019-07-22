// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/system_media_controls_notifier.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/time/time.h"
#include "content/public/browser/content_browser_client.h"
#include "services/media_session/public/mojom/constants.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/idle/idle.h"
#include "ui/base/win/system_media_controls/system_media_controls_service.h"

namespace content {

using ABI::Windows::Media::MediaPlaybackStatus;

const int kMinImageSize = 71;
const int kDesiredImageSize = 150;

constexpr base::TimeDelta kScreenLockPollInterval =
    base::TimeDelta::FromSeconds(1);
constexpr int kHideSmtcDelaySeconds = 5;
constexpr base::TimeDelta kHideSmtcDelay =
    base::TimeDelta::FromSeconds(kHideSmtcDelaySeconds);

SystemMediaControlsNotifier::SystemMediaControlsNotifier(
    service_manager::Connector* connector)
    : connector_(connector) {}

SystemMediaControlsNotifier::~SystemMediaControlsNotifier() = default;

void SystemMediaControlsNotifier::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // |service_| can be set in tests.
  if (!service_)
    service_ = system_media_controls::SystemMediaControlsService::GetInstance();

  // |service_| can still be null if the current system does not support System
  // Media Transport Controls.
  if (!service_)
    return;

  lock_polling_timer_.Start(
      FROM_HERE, kScreenLockPollInterval,
      base::BindRepeating(&SystemMediaControlsNotifier::CheckLockState,
                          base::Unretained(this)));

  // |connector_| can be null in tests.
  if (!connector_)
    return;

  // Connect to the MediaControllerManager and create a MediaController that
  // controls the active session so we can observe it.
  media_session::mojom::MediaControllerManagerPtr controller_manager_ptr;
  connector_->BindInterface(media_session::mojom::kServiceName,
                            mojo::MakeRequest(&controller_manager_ptr));
  controller_manager_ptr->CreateActiveMediaController(
      mojo::MakeRequest(&media_controller_ptr_));

  // Observe the active media controller for changes to playback state and
  // supported actions.
  media_controller_ptr_->AddObserver(
      media_controller_observer_receiver_.BindNewPipeAndPassRemote());

  // Observe the active media controller for changes to provided artwork.
  media_controller_ptr_->ObserveImages(
      media_session::mojom::MediaSessionImageType::kArtwork, kMinImageSize,
      kDesiredImageSize,
      media_controller_image_observer_receiver_.BindNewPipeAndPassRemote());
}

void SystemMediaControlsNotifier::CheckLockState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool new_state = ui::CheckIdleStateIsLocked();
  if (screen_locked_ == new_state)
    return;

  screen_locked_ = new_state;
  if (screen_locked_)
    OnScreenLocked();
  else
    OnScreenUnlocked();
}

void SystemMediaControlsNotifier::OnScreenLocked() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(service_);

  // If media is currently playing, don't hide the SMTC.
  if (session_info_ptr_ &&
      session_info_ptr_->playback_state ==
          media_session::mojom::MediaPlaybackState::kPlaying) {
    return;
  }

  // Otherwise, hide them.
  service_->SetEnabled(false);
}

void SystemMediaControlsNotifier::OnScreenUnlocked() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(service_);

  StopHideSmtcTimer();
  service_->SetEnabled(true);
}

void SystemMediaControlsNotifier::StartHideSmtcTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  hide_smtc_timer_.Start(
      FROM_HERE, kHideSmtcDelay,
      base::BindOnce(&SystemMediaControlsNotifier::HideSmtcTimerFired,
                     base::Unretained(this)));
}

void SystemMediaControlsNotifier::StopHideSmtcTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  hide_smtc_timer_.Stop();
}

void SystemMediaControlsNotifier::HideSmtcTimerFired() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(service_);

  service_->SetEnabled(false);
}

void SystemMediaControlsNotifier::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr session_info_ptr) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(service_);

  bool is_playing = false;

  session_info_ptr_ = std::move(session_info_ptr);
  if (session_info_ptr_) {
    if (session_info_ptr_->playback_state ==
        media_session::mojom::MediaPlaybackState::kPlaying) {
      is_playing = true;
      service_->SetPlaybackStatus(
          MediaPlaybackStatus::MediaPlaybackStatus_Playing);
    } else {
      service_->SetPlaybackStatus(
          MediaPlaybackStatus::MediaPlaybackStatus_Paused);
    }
  } else {
    service_->SetPlaybackStatus(
        MediaPlaybackStatus::MediaPlaybackStatus_Stopped);

    // These steps reference the Media Session Standard
    // https://wicg.github.io/mediasession/#metadata
    // 5.3.1 If the active media session is null, unset the media metadata
    // presented to the platform, and terminate these steps.
    service_->ClearMetadata();
  }

  if (screen_locked_) {
    if (is_playing)
      StopHideSmtcTimer();
    else if (!hide_smtc_timer_.IsRunning())
      StartHideSmtcTimer();
  }
}

void SystemMediaControlsNotifier::MediaSessionMetadataChanged(
    const base::Optional<media_session::MediaMetadata>& metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(service_);

  if (metadata.has_value()) {
    // 5.3.3 Update the media metadata presented to the platform to match the
    // metadata for the active media session.
    // If no title was provided, the title of the tab will be in the title
    // property.
    service_->SetTitle(metadata->title);

    // If no artist was provided, then the source URL will be in the artist
    // property.
    service_->SetArtist(metadata->artist);
    service_->UpdateDisplay();
  } else {
    // 5.3.2 If the metadata of the active media session is an empty metadata,
    // unset the media metadata presented to the platform.
    service_->ClearMetadata();
  }
}

void SystemMediaControlsNotifier::MediaControllerImageChanged(
    media_session::mojom::MediaSessionImageType type,
    const SkBitmap& bitmap) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(service_);

  if (!bitmap.empty()) {
    // 5.3.4.4.3 If the image format is supported, use the image as the artwork
    // for display in the platform UI. Otherwise the fetch image algorithm fails
    // and terminates.
    service_->SetThumbnail(bitmap);
  } else {
    // 5.3.4.2 If metadata's artwork is empty, terminate these steps.
    // If no images are fetched in the fetch image algorithm, the user agent
    // may have fallback behavior such as displaying a default image as artwork.
    // We display the application icon if no artwork is provided.
    base::Optional<gfx::ImageSkia> icon =
        GetContentClient()->browser()->GetProductLogo();
    if (icon.has_value())
      service_->SetThumbnail(*icon->bitmap());
    else
      service_->ClearThumbnail();
  }
}

}  // namespace content
