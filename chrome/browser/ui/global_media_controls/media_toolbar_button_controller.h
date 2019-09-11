// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_CONTROLLER_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_CONTROLLER_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/media_message_center/media_notification_controller.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"

namespace content {
class WebContents;
}  // namespace content

namespace media_message_center {
class MediaNotificationItem;
}  // namespace media_message_center

namespace service_manager {
class Connector;
}  // namespace service_manager

class MediaDialogDelegate;
class MediaToolbarButtonControllerDelegate;

// Controller for the MediaToolbarButtonView that decides when to show or hide
// the icon from the toolbar. Also passes MediaNotificationItems to the
// MediaDialogView to display.
class MediaToolbarButtonController
    : public media_session::mojom::AudioFocusObserver,
      public media_message_center::MediaNotificationController {
 public:
  MediaToolbarButtonController(const base::UnguessableToken& source_id,
                               service_manager::Connector* connector,
                               MediaToolbarButtonControllerDelegate* delegate);
  ~MediaToolbarButtonController() override;

  // media_session::mojom::AudioFocusObserver implementation.
  void OnFocusGained(
      media_session::mojom::AudioFocusRequestStatePtr session) override;
  void OnFocusLost(
      media_session::mojom::AudioFocusRequestStatePtr session) override;

  // media_message_center::MediaNotificationController implementation.
  void ShowNotification(const std::string& id) override;
  void HideNotification(const std::string& id) override;
  void RemoveItem(const std::string& id) override;
  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner() const override;
  void LogMediaSessionActionButtonPressed(const std::string& id) override;

  void SetDialogDelegate(MediaDialogDelegate* delegate);

  // Called when the dismiss button was clicked on a session.
  void OnDismissButtonClicked(const std::string& id);

 private:
  friend class MediaToolbarButtonControllerTest;

  // Tracks the current display state of the toolbar button delegate.
  enum class DisplayState {
    kShown,
    kDisabled,
    kHidden,
  };

  class Session : public content::WebContentsObserver {
   public:
    Session(MediaToolbarButtonController* owner,
            const std::string& id,
            std::unique_ptr<media_message_center::MediaNotificationItem> item,
            content::WebContents* web_contents);
    ~Session() override;

    // content::WebContentsObserver implementation.
    void WebContentsDestroyed() override;

    media_message_center::MediaNotificationItem* item() { return item_.get(); }

   private:
    MediaToolbarButtonController* owner_;
    const std::string id_;
    std::unique_ptr<media_message_center::MediaNotificationItem> item_;

    DISALLOW_COPY_AND_ASSIGN(Session);
  };

  void OnReceivedAudioFocusRequests(
      std::vector<media_session::mojom::AudioFocusRequestStatePtr> sessions);

  void UpdateToolbarButtonState();

  service_manager::Connector* const connector_;
  MediaToolbarButtonControllerDelegate* const delegate_;
  MediaDialogDelegate* dialog_delegate_ = nullptr;

  // The delegate starts hidden and isn't shown until media playback starts.
  DisplayState delegate_display_state_ = DisplayState::kHidden;

  // Used to track whether there are any active controllable media sessions. If
  // not, then there's nothing to show in the dialog and we can hide the toolbar
  // icon.
  std::unordered_set<std::string> active_controllable_session_ids_;

  // Tracks the sessions that are currently frozen. If there are only frozen
  // sessions, we will disable the toolbar icon and wait to hide it.
  std::unordered_set<std::string> frozen_session_ids_;

  // Stores a Session for each media session keyed by its |request_id| in string
  // format.
  std::map<const std::string, Session> sessions_;

  // Connections with the media session service to listen for audio focus
  // updates and control media sessions.
  mojo::Remote<media_session::mojom::AudioFocusManager> audio_focus_remote_;
  mojo::Remote<media_session::mojom::MediaControllerManager>
      controller_manager_remote_;
  mojo::Receiver<media_session::mojom::AudioFocusObserver>
      audio_focus_observer_receiver_{this};

  base::WeakPtrFactory<MediaToolbarButtonController> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MediaToolbarButtonController);
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_TOOLBAR_BUTTON_CONTROLLER_H_
