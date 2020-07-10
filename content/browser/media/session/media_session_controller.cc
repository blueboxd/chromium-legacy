// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/media_session_controller.h"

#include "content/browser/media/media_web_contents_observer.h"
#include "content/browser/media/session/media_session_impl.h"
#include "content/common/media/media_player_delegate_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "media/base/media_content_type.h"

namespace content {

MediaSessionController::MediaSessionController(
    const MediaPlayerId& id,
    MediaWebContentsObserver* media_web_contents_observer)
    : id_(id),
      media_web_contents_observer_(media_web_contents_observer),
      media_session_(
          MediaSessionImpl::Get(media_web_contents_observer_->web_contents())) {
}

MediaSessionController::~MediaSessionController() {
  media_session_->RemovePlayer(this, player_id_);
}

bool MediaSessionController::OnPlaybackStarted(
    bool has_audio,
    bool has_video,
    media::MediaContentType media_content_type) {
  is_playback_in_progress_ = true;

  // Store these as we will need them later.
  has_audio_ = has_audio;
  has_video_ = has_video;
  media_content_type_ = media_content_type;

  // Don't generate a new id if one has already been set.
  if (!has_session_) {
    // These objects are only created on the UI thread, so this is safe.
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    static uint32_t player_id = 0;
    player_id_ = static_cast<int>(player_id++);
  }

  // Don't bother with a MediaSession for remote players or without audio.  If
  // we already have a session from a previous call, release it.
  if (!IsMediaSessionNeeded()) {
    has_session_ = false;
    media_session_->RemovePlayer(this, player_id_);
    return true;
  }

  // If a session can't be created, force a pause immediately.  Attempt to add a
  // session even if we already have one.  MediaSession expects AddPlayer() to
  // be called after OnPlaybackPaused() to reactivate the session.
  if (!media_session_->AddPlayer(this, player_id_, media_content_type)) {
    OnSuspend(player_id_);
    return false;
  }

  has_session_ = true;
  return true;
}

void MediaSessionController::OnSuspend(int player_id) {
  DCHECK_EQ(player_id_, player_id);
  // TODO(crbug.com/953645): Set triggered_by_user to true ONLY if that action
  // was actually triggered by user as this will activate the frame.
  id_.render_frame_host->Send(new MediaPlayerDelegateMsg_Pause(
      id_.render_frame_host->GetRoutingID(), id_.delegate_id,
      true /* triggered_by_user */));
}

void MediaSessionController::OnResume(int player_id) {
  DCHECK_EQ(player_id_, player_id);
  id_.render_frame_host->Send(new MediaPlayerDelegateMsg_Play(
      id_.render_frame_host->GetRoutingID(), id_.delegate_id));
}

void MediaSessionController::OnSeekForward(int player_id,
                                           base::TimeDelta seek_time) {
  DCHECK_EQ(player_id_, player_id);
  id_.render_frame_host->Send(new MediaPlayerDelegateMsg_SeekForward(
      id_.render_frame_host->GetRoutingID(), id_.delegate_id, seek_time));
}

void MediaSessionController::OnSeekBackward(int player_id,
                                            base::TimeDelta seek_time) {
  DCHECK_EQ(player_id_, player_id);
  id_.render_frame_host->Send(new MediaPlayerDelegateMsg_SeekBackward(
      id_.render_frame_host->GetRoutingID(), id_.delegate_id, seek_time));
}

void MediaSessionController::OnSetVolumeMultiplier(int player_id,
                                                   double volume_multiplier) {
  DCHECK_EQ(player_id_, player_id);
  id_.render_frame_host->Send(new MediaPlayerDelegateMsg_UpdateVolumeMultiplier(
      id_.render_frame_host->GetRoutingID(), id_.delegate_id,
      volume_multiplier));
}

void MediaSessionController::OnEnterPictureInPicture(int player_id) {
  DCHECK_EQ(player_id_, player_id);
  id_.render_frame_host->Send(new MediaPlayerDelegateMsg_EnterPictureInPicture(
      id_.render_frame_host->GetRoutingID(), id_.delegate_id));
}

void MediaSessionController::OnExitPictureInPicture(int player_id) {
  DCHECK_EQ(player_id_, player_id);
  id_.render_frame_host->Send(new MediaPlayerDelegateMsg_ExitPictureInPicture(
      id_.render_frame_host->GetRoutingID(), id_.delegate_id));
}

RenderFrameHost* MediaSessionController::render_frame_host() const {
  return id_.render_frame_host;
}

base::Optional<media_session::MediaPosition>
MediaSessionController::GetPosition(int player_id) const {
  DCHECK_EQ(player_id_, player_id);
  return position_;
}

bool MediaSessionController::IsPictureInPictureAvailable(int player_id) const {
  DCHECK_EQ(player_id_, player_id);
  return is_picture_in_picture_available_;
}

void MediaSessionController::OnPlaybackPaused(bool reached_end_of_stream) {
  if (reached_end_of_stream) {
    is_playback_in_progress_ = false;
    AddOrRemovePlayer();
  }

  // We check for suspension here since the renderer may issue its own pause
  // in response to or while a pause from the browser is in flight.
  if (media_session_->IsActive())
    media_session_->OnPlayerPaused(this, player_id_);
}

void MediaSessionController::PictureInPictureStateChanged(
    bool is_picture_in_picture) {
  AddOrRemovePlayer();
}

void MediaSessionController::WebContentsMutedStateChanged(bool muted) {
  AddOrRemovePlayer();
}

void MediaSessionController::OnMediaPositionStateChanged(
    const media_session::MediaPosition& position) {
  position_ = position;
  media_session_->RebuildAndNotifyMediaPositionChanged();
}

void MediaSessionController::OnPictureInPictureAvailabilityChanged(
    bool available) {
  is_picture_in_picture_available_ = available;
  media_session_->OnPictureInPictureAvailabilityChanged();
}

bool MediaSessionController::IsMediaSessionNeeded() const {
  if (!is_playback_in_progress_)
    return false;

  // We want to make sure we do not request audio focus on a muted tab as it
  // would break user expectations by pausing/ducking other playbacks.
  const bool has_audio =
      has_audio_ &&
      !media_web_contents_observer_->web_contents()->IsAudioMuted();
  return has_audio || media_web_contents_observer_->web_contents()
                          ->HasPictureInPictureVideo();
}

void MediaSessionController::AddOrRemovePlayer() {
  const bool needs_session = IsMediaSessionNeeded();
  if (needs_session && !has_session_) {
    has_session_ =
        media_session_->AddPlayer(this, player_id_, media_content_type_);
  } else if (!needs_session && has_session_) {
    has_session_ = false;
    media_session_->RemovePlayer(this, player_id_);
  }
}

bool MediaSessionController::HasVideo(int player_id) const {
  DCHECK_EQ(player_id_, player_id);
  return has_video_ && has_audio_;
}

}  // namespace content
