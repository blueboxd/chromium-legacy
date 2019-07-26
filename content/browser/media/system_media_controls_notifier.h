// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_SYSTEM_MEDIA_CONTROLS_NOTIFIER_H_
#define CONTENT_BROWSER_MEDIA_SYSTEM_MEDIA_CONTROLS_NOTIFIER_H_

#include <memory>
#include <vector>

#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"

namespace system_media_controls {
class SystemMediaControlsService;
}  // namespace system_media_controls

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace content {

// The SystemMediaControlsNotifier connects to Window's "System Media Transport
// Controls" and keeps the OS informed of the current media playstate and
// metadata. It observes changes to the active Media Session and updates the
// SMTC accordingly.
class CONTENT_EXPORT SystemMediaControlsNotifier
    : public media_session::mojom::MediaControllerObserver,
      public media_session::mojom::MediaControllerImageObserver {
 public:
  explicit SystemMediaControlsNotifier(service_manager::Connector* connector);
  ~SystemMediaControlsNotifier() override;

  void Initialize();

  // media_session::mojom::MediaControllerObserver implementation.
  void MediaSessionInfoChanged(
      media_session::mojom::MediaSessionInfoPtr session_info) override;
  void MediaSessionMetadataChanged(
      const base::Optional<media_session::MediaMetadata>& metadata) override;
  void MediaSessionActionsChanged(
      const std::vector<media_session::mojom::MediaSessionAction>& actions)
      override {}
  void MediaSessionChanged(
      const base::Optional<base::UnguessableToken>& request_id) override {}
  void MediaSessionPositionChanged(
      const base::Optional<media_session::MediaPosition>& position) override {}

  // media_session::mojom::MediaControllerImageObserver implementation.
  void MediaControllerImageChanged(
      ::media_session::mojom::MediaSessionImageType type,
      const SkBitmap& bitmap) override;

  void SetSystemMediaControlsServiceForTesting(
      system_media_controls::SystemMediaControlsService* service) {
    service_ = service;
  }

 private:
  friend class SystemMediaControlsNotifierTest;

  // Polls the current idle state of the system.
  void CheckLockState();

  // Called when the idle state changes from unlocked to locked.
  void OnScreenLocked();

  // Called when the idle state changes from locked to unlocked.
  void OnScreenUnlocked();

  // Helper functions for dealing with the timer that hides the System Media
  // Transport Controls on the lock screen 5 seconds after the user pauses.
  void StartHideSmtcTimer();
  void StopHideSmtcTimer();
  void HideSmtcTimerFired();

  bool screen_locked_ = false;
  base::RepeatingTimer lock_polling_timer_;
  base::OneShotTimer hide_smtc_timer_;

  // Our connection to Window's System Media Transport Controls.
  system_media_controls::SystemMediaControlsService* service_ = nullptr;

  // used to connect to the Media Session service.
  service_manager::Connector* connector_;

  // Tracks current media session state/metadata.
  media_session::mojom::MediaControllerPtr media_controller_ptr_;
  media_session::mojom::MediaSessionInfoPtr session_info_ptr_;

  // Used to receive updates to the active media controller.
  mojo::Receiver<media_session::mojom::MediaControllerObserver>
      media_controller_observer_receiver_{this};
  mojo::Receiver<media_session::mojom::MediaControllerImageObserver>
      media_controller_image_observer_receiver_{this};

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(SystemMediaControlsNotifier);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_SYSTEM_MEDIA_CONTROLS_NOTIFIER_H_
