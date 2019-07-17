// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_DIALOG_CONTROLLER_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/media_message_center/media_notification_controller.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"

namespace media_message_center {
class MediaNotificationItem;
}  // namespace media_message_center

namespace service_manager {
class Connector;
}  // namespace service_manager

class MediaDialogControllerDelegate;

// Controller for the MediaDialogView that updates the view when the active
// media session changes.
class MediaDialogController
    : public media_session::mojom::AudioFocusObserver,
      public media_message_center::MediaNotificationController {
 public:
  MediaDialogController(service_manager::Connector* connector,
                        MediaDialogControllerDelegate* delegate);
  ~MediaDialogController() override;

  void Initialize();

  // media_session::mojom::AudioFocusObserver implementation.
  void OnFocusGained(
      media_session::mojom::AudioFocusRequestStatePtr session) override;
  void OnFocusLost(
      media_session::mojom::AudioFocusRequestStatePtr session) override;

  // media_message_center::MediaNotificationController implementation.
  void ShowNotification(const std::string& id) override;
  void HideNotification(const std::string& id) override;

 private:
  friend class MediaDialogControllerTest;

  // Called when we receive all currently active media sessions for the audio
  // focus manager. Used to initialize the list of sessions.
  void OnReceivedAudioFocusRequests(
      std::vector<media_session::mojom::AudioFocusRequestStatePtr> sessions);

  service_manager::Connector* const connector_;
  MediaDialogControllerDelegate* const delegate_;

  // Stores a |media_message_center::MediaNotificationItem| for each media
  // session keyed by its |request_id| in string format.
  std::map<const std::string, media_message_center::MediaNotificationItem>
      sessions_;

  media_session::mojom::AudioFocusManagerPtr audio_focus_ptr_;
  media_session::mojom::MediaControllerManagerPtr controller_manager_ptr_;

  // Used to receive updates to the active media controller.
  mojo::Receiver<media_session::mojom::AudioFocusObserver>
      audio_focus_observer_receiver_{this};

  base::WeakPtrFactory<MediaDialogController> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MediaDialogController);
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_DIALOG_CONTROLLER_H_
