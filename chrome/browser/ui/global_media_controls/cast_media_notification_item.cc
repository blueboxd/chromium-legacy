// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/cast_media_notification_item.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/global_media_controls/cast_media_session_controller.h"
#include "components/media_message_center/media_notification_controller.h"
#include "components/media_message_center/media_notification_view.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace {

media_session::mojom::MediaSessionInfoPtr CreateSessionInfo() {
  auto session_info = media_session::mojom::MediaSessionInfo::New();
  session_info->state =
      media_session::mojom::MediaSessionInfo::SessionState::kSuspended;
  session_info->force_duck = false;
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPaused;
  session_info->is_controllable = true;
  session_info->prefer_stop_for_gain_focus_loss = false;
  return session_info;
}

std::vector<media_session::mojom::MediaSessionAction> ToMediaSessionActions(
    const media_router::mojom::MediaStatus& status) {
  std::vector<media_session::mojom::MediaSessionAction> actions;
  // |can_mute| and |can_set_volume| have no MediaSessionAction equivalents.
  if (status.can_play_pause) {
    actions.push_back(media_session::mojom::MediaSessionAction::kPlay);
    actions.push_back(media_session::mojom::MediaSessionAction::kPause);
  }
  if (status.can_seek) {
    actions.push_back(media_session::mojom::MediaSessionAction::kSeekBackward);
    actions.push_back(media_session::mojom::MediaSessionAction::kSeekForward);
    actions.push_back(media_session::mojom::MediaSessionAction::kSeekTo);
  }
  if (status.can_skip_to_next_track) {
    actions.push_back(media_session::mojom::MediaSessionAction::kNextTrack);
  }
  if (status.can_skip_to_previous_track) {
    actions.push_back(media_session::mojom::MediaSessionAction::kPreviousTrack);
  }
  return actions;
}

media_session::mojom::MediaPlaybackState ToPlaybackState(
    media_router::mojom::MediaStatus::PlayState play_state) {
  switch (play_state) {
    case media_router::mojom::MediaStatus::PlayState::PLAYING:
      return media_session::mojom::MediaPlaybackState::kPlaying;
    case media_router::mojom::MediaStatus::PlayState::PAUSED:
      return media_session::mojom::MediaPlaybackState::kPaused;
    case media_router::mojom::MediaStatus::PlayState::BUFFERING:
      return media_session::mojom::MediaPlaybackState::kPlaying;
  }
}

media_session::mojom::MediaSessionInfo::SessionState ToSessionState(
    media_router::mojom::MediaStatus::PlayState play_state) {
  switch (play_state) {
    case media_router::mojom::MediaStatus::PlayState::PLAYING:
      return media_session::mojom::MediaSessionInfo::SessionState::kActive;
    case media_router::mojom::MediaStatus::PlayState::PAUSED:
      return media_session::mojom::MediaSessionInfo::SessionState::kSuspended;
    case media_router::mojom::MediaStatus::PlayState::BUFFERING:
      return media_session::mojom::MediaSessionInfo::SessionState::kActive;
  }
}

}  // namespace

CastMediaNotificationItem::CastMediaNotificationItem(
    const media_router::MediaRoute& route,
    media_message_center::MediaNotificationController* notification_controller,
    std::unique_ptr<CastMediaSessionController> session_controller)
    : notification_controller_(notification_controller),
      session_controller_(std::move(session_controller)),
      media_route_id_(route.media_route_id()),
      session_info_(CreateSessionInfo()) {
  metadata_.artist = base::UTF8ToUTF16(route.description());
  notification_controller_->ShowNotification(media_route_id_);
}

CastMediaNotificationItem::~CastMediaNotificationItem() {
  notification_controller_->HideNotification(media_route_id_);
}

void CastMediaNotificationItem::SetView(
    media_message_center::MediaNotificationView* view) {
  view_ = view;
  UpdateView();
}

void CastMediaNotificationItem::OnMediaSessionActionButtonPressed(
    media_session::mojom::MediaSessionAction action) {
  session_controller_->Send(action);
}

void CastMediaNotificationItem::Dismiss() {
  notification_controller_->HideNotification(media_route_id_);
}

void CastMediaNotificationItem::OnMediaStatusUpdated(
    media_router::mojom::MediaStatusPtr status) {
  metadata_.title = base::UTF8ToUTF16(status->title);
  actions_ = ToMediaSessionActions(*status);
  session_info_->state = ToSessionState(status->play_state);
  session_info_->playback_state = ToPlaybackState(status->play_state);

  // TODO(crbug.com/987479): Fetch and set the background image.
  UpdateView();
  session_controller_->OnMediaStatusUpdated(std::move(status));
}

mojo::PendingRemote<media_router::mojom::MediaStatusObserver>
CastMediaNotificationItem::GetObserverPendingRemote() {
  return observer_receiver_.BindNewPipeAndPassRemote();
}

void CastMediaNotificationItem::UpdateView() {
  if (!view_)
    return;

  view_->UpdateWithMediaMetadata(metadata_);
  view_->UpdateWithMediaActions(actions_);
  view_->UpdateWithMediaSessionInfo(session_info_.Clone());
}
