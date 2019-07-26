// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_CONTROLLER_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_CONTROLLER_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

class MediaToolbarButtonControllerDelegate;

// Controller for the MediaToolbarButtonView that decides when to show or hide
// the icon from the toolbar.
class MediaToolbarButtonController
    : public media_session::mojom::MediaControllerObserver {
 public:
  MediaToolbarButtonController(service_manager::Connector* connector,
                               MediaToolbarButtonControllerDelegate* delegate);
  ~MediaToolbarButtonController() override;

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
  service_manager::Connector* const connector_;
  MediaToolbarButtonControllerDelegate* const delegate_;

  // Tracks current media session state/metadata.
  media_session::mojom::MediaControllerPtr media_controller_ptr_;

  // Used to receive updates to the active media controller.
  mojo::Receiver<media_session::mojom::MediaControllerObserver>
      media_controller_observer_receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(MediaToolbarButtonController);
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_CONTROLLER_H_
