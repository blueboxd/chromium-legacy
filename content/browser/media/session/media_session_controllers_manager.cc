// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/media_session_controllers_manager.h"

#include "base/stl_util.h"
#include "content/browser/media/session/media_session_controller.h"
#include "media/base/media_switches.h"
#include "services/media_session/public/cpp/features.h"

namespace content {

namespace {

bool IsMediaSessionEnabled() {
  return base::FeatureList::IsEnabled(
             media_session::features::kMediaSessionService) ||
         base::FeatureList::IsEnabled(media::kInternalMediaSession);
}

}  // namespace

MediaSessionControllersManager::MediaSessionControllersManager(
    MediaWebContentsObserver* media_web_contents_observer)
    : media_web_contents_observer_(media_web_contents_observer) {}

MediaSessionControllersManager::~MediaSessionControllersManager() = default;

void MediaSessionControllersManager::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  if (!IsMediaSessionEnabled())
    return;

  base::EraseIf(
      controllers_map_,
      [render_frame_host](const ControllersMap::value_type& id_and_controller) {
        return render_frame_host == id_and_controller.first.render_frame_host;
      });
}

bool MediaSessionControllersManager::RequestPlay(
    const MediaPlayerId& id,
    bool has_audio,
    media::MediaContentType media_content_type,
    bool has_video) {
  if (!IsMediaSessionEnabled())
    return true;

  // Since we don't remove session instances on pause, there may be an existing
  // instance for this playback attempt.  In this case, try to reinitialize it
  // with the new settings.  If they are the same, this is a no-op.
  MediaSessionController* const controller = FindOrCreateController(id);
  return controller->OnPlaybackStarted(has_audio, has_video,
                                       media_content_type);
}

void MediaSessionControllersManager::OnPause(const MediaPlayerId& id) {
  if (!IsMediaSessionEnabled())
    return;

  MediaSessionController* const controller = FindOrCreateController(id);
  controller->OnPlaybackPaused(false);
}

void MediaSessionControllersManager::OnEnd(const MediaPlayerId& id) {
  if (!IsMediaSessionEnabled())
    return;

  // TODO(wdzierzanowski): OnEnd() currently doubles as signal that playback
  // has ended and that the player has been destroyed.  Replace the following
  // call with removing the controller from the map once OnEnd() is only issued
  // on player destruction.  https://crbug.com/1091203
  MediaSessionController* const controller = FindOrCreateController(id);
  controller->OnPlaybackPaused(true);
}

void MediaSessionControllersManager::OnMediaPositionStateChanged(
    const MediaPlayerId& id,
    const media_session::MediaPosition& position) {
  if (!IsMediaSessionEnabled())
    return;

  MediaSessionController* const controller = FindOrCreateController(id);
  controller->OnMediaPositionStateChanged(position);
}

void MediaSessionControllersManager::PictureInPictureStateChanged(
    bool is_picture_in_picture) {
  if (!IsMediaSessionEnabled())
    return;

  for (auto& entry : controllers_map_)
    entry.second->PictureInPictureStateChanged(is_picture_in_picture);
}

void MediaSessionControllersManager::WebContentsMutedStateChanged(bool muted) {
  if (!IsMediaSessionEnabled())
    return;

  for (auto& entry : controllers_map_)
    entry.second->WebContentsMutedStateChanged(muted);
}

void MediaSessionControllersManager::OnPictureInPictureAvailabilityChanged(
    const MediaPlayerId& id,
    bool available) {
  if (!IsMediaSessionEnabled())
    return;

  MediaSessionController* const controller = FindOrCreateController(id);
  controller->OnPictureInPictureAvailabilityChanged(available);
}

MediaSessionController* MediaSessionControllersManager::FindOrCreateController(
    const MediaPlayerId& id) {
  auto it = controllers_map_.find(id);
  if (it == controllers_map_.end()) {
    it = controllers_map_
             .emplace(id, std::make_unique<MediaSessionController>(
                              id, media_web_contents_observer_))
             .first;
  }
  return it->second.get();
}

}  // namespace content
