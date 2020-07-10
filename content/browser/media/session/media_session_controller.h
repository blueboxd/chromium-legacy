// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_CONTROLLER_H_
#define CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_CONTROLLER_H_

#include "base/compiler_specific.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "content/browser/media/session/media_session_player_observer.h"
#include "content/common/content_export.h"
#include "content/public/browser/media_player_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "media/base/media_content_type.h"
#include "services/media_session/public/cpp/media_position.h"

namespace content {

class MediaSessionImpl;
class MediaWebContentsObserver;

// Helper class for controlling a single player's MediaSession instance.  Sends
// browser side MediaSession commands back to a player hosted in the renderer
// process.
class CONTENT_EXPORT MediaSessionController
    : public MediaSessionPlayerObserver {
 public:
  MediaSessionController(const MediaPlayerId& id,
                         MediaWebContentsObserver* media_web_contents_observer);
  ~MediaSessionController() override;

  // Must be called when playback starts. May be called more than once; does
  // nothing if none of the input parameters have changed since the last call.
  // Returns false if a media session cannot be created.
  bool OnPlaybackStarted(bool has_audio,
                         bool has_video,
                         media::MediaContentType media_content_type);

  // Must be called when a pause occurs on the renderer side media player; keeps
  // the MediaSession instance in sync with renderer side behavior.
  void OnPlaybackPaused(bool reached_end_of_stream);

  // MediaSessionObserver implementation.
  void OnSuspend(int player_id) override;
  void OnResume(int player_id) override;
  void OnSeekForward(int player_id, base::TimeDelta seek_time) override;
  void OnSeekBackward(int player_id, base::TimeDelta seek_time) override;
  void OnSetVolumeMultiplier(int player_id, double volume_multiplier) override;
  void OnEnterPictureInPicture(int player_id) override;
  void OnExitPictureInPicture(int player_id) override;
  RenderFrameHost* render_frame_host() const override;
  base::Optional<media_session::MediaPosition> GetPosition(
      int player_id) const override;
  bool IsPictureInPictureAvailable(int player_id) const override;
  bool HasVideo(int player_id) const override;

  // Test helpers.
  int get_player_id_for_testing() const { return player_id_; }

  // Called when entering/leaving Picture-in-Picture for the given media
  // player.
  void PictureInPictureStateChanged(bool is_picture_in_picture);

  // Called when the WebContents is either muted or unmuted.
  void WebContentsMutedStateChanged(bool muted);

  // Called when the media position state of the player has changed.
  void OnMediaPositionStateChanged(
      const media_session::MediaPosition& position);

  // Called when the media picture-in-picture availability has changed.
  void OnPictureInPictureAvailabilityChanged(bool available);

  bool IsMediaSessionNeeded() const;

  // Determines whether a session is needed and adds or removes the player
  // accordingly.
  void AddOrRemovePlayer();

 private:
  const MediaPlayerId id_;

  // Non-owned pointer; |media_web_contents_observer_| is the owner of |this|.
  MediaWebContentsObserver* const media_web_contents_observer_;

  // Non-owned pointer; lifetime is the same as |media_web_contents_observer_|.
  MediaSessionImpl* const media_session_;

  base::Optional<media_session::MediaPosition> position_;

  int player_id_ = 0;
  bool has_session_ = false;
  // Playing or paused, but not ended.
  bool is_playback_in_progress_ = false;
  bool has_audio_ = false;
  bool has_video_ = false;
  bool is_picture_in_picture_available_ = false;
  media::MediaContentType media_content_type_ =
      media::MediaContentType::Persistent;

  DISALLOW_COPY_AND_ASSIGN(MediaSessionController);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_CONTROLLER_H_
