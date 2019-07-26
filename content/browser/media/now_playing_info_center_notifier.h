// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_NOW_PLAYING_INFO_CENTER_NOTIFIER_H_
#define CONTENT_BROWSER_MEDIA_NOW_PLAYING_INFO_CENTER_NOTIFIER_H_

#include <memory>
#include <vector>

#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"

namespace now_playing {
class NowPlayingInfoCenterDelegate;
}  // namespace now_playing

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace content {

// The NowPlayingInfoCenterNotifier connects to Mac OS's "Now Playing" info
// center and keeps the OS informed of the current media playstate and metadata.
// It observes changes to the active Media Session and updates the info center
// accordingly.
class CONTENT_EXPORT NowPlayingInfoCenterNotifier
    : public media_session::mojom::MediaControllerObserver {
 public:
  NowPlayingInfoCenterNotifier(
      service_manager::Connector* connector,
      std::unique_ptr<now_playing::NowPlayingInfoCenterDelegate>
          now_playing_info_center_delegate);
  ~NowPlayingInfoCenterNotifier() override;

  // media_session::mojom::MediaControllerObserver implementation.
  void MediaSessionInfoChanged(
      media_session::mojom::MediaSessionInfoPtr session_info) override;
  void MediaSessionMetadataChanged(
      const base::Optional<media_session::MediaMetadata>& metadata) override {}
  void MediaSessionActionsChanged(
      const std::vector<media_session::mojom::MediaSessionAction>& actions)
      override {}
  void MediaSessionChanged(
      const base::Optional<base::UnguessableToken>& request_id) override {}
  void MediaSessionPositionChanged(
      const base::Optional<media_session::MediaPosition>& position) override {}

 private:
  // Our connection to the underlying OS API for MPNowPlayingInfoCenter.
  std::unique_ptr<now_playing::NowPlayingInfoCenterDelegate>
      now_playing_info_center_delegate_;

  // Tracks current media session state/metadata.
  media_session::mojom::MediaControllerPtr media_controller_ptr_;
  media_session::mojom::MediaSessionInfoPtr session_info_;

  // Used to receive updates to the active media controller.
  mojo::Receiver<media_session::mojom::MediaControllerObserver>
      media_controller_observer_receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(NowPlayingInfoCenterNotifier);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_NOW_PLAYING_INFO_CENTER_NOTIFIER_H_
