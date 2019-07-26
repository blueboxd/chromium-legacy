// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOCK_SCREEN_MEDIA_CONTROLS_VIEW_H_
#define ASH_LOGIN_UI_LOCK_SCREEN_MEDIA_CONTROLS_VIEW_H_

#include <set>

#include "ash/ash_export.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "ui/views/controls/button/button.h"

namespace service_manager {
class Connector;
}

namespace views {
class ImageView;
class ToggleImageButton;
class ImageButton;
}  // namespace views

namespace ash {

class MediaControlsHeaderView;
class NonAccessibleView;

class ASH_EXPORT LockScreenMediaControlsView
    : public views::View,
      public media_session::mojom::MediaControllerObserver,
      public media_session::mojom::MediaControllerImageObserver,
      public views::ButtonListener {
 public:
  using MediaControlsEnabled = base::RepeatingCallback<bool()>;

  struct Callbacks {
    Callbacks();
    ~Callbacks();

    // Called in |MediaSessionInfoChanged| to determine the visibility of the
    // media controls.
    MediaControlsEnabled media_controls_enabled;

    // Called when the controls should be hidden on the lock screen.
    base::RepeatingClosure hide_media_controls;

    // Called when the controls should be drawn on the lock screen.
    base::RepeatingClosure show_media_controls;

    DISALLOW_COPY_AND_ASSIGN(Callbacks);
  };

  LockScreenMediaControlsView(service_manager::Connector* connector,
                              const Callbacks& callbacks);
  ~LockScreenMediaControlsView() override;

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

  views::View* GetMiddleSpacingView();

  // media_session::mojom::MediaControllerObserver:
  void MediaSessionInfoChanged(
      media_session::mojom::MediaSessionInfoPtr session_info) override;
  void MediaSessionMetadataChanged(
      const base::Optional<media_session::MediaMetadata>& metadata) override;
  void MediaSessionActionsChanged(
      const std::vector<media_session::mojom::MediaSessionAction>& actions)
      override;
  void MediaSessionChanged(
      const base::Optional<base::UnguessableToken>& request_id) override;
  void MediaSessionPositionChanged(
      const base::Optional<media_session::MediaPosition>& position) override {}

  // media_session::mojom::MediaControllerImageObserver:
  void MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType type,
      const SkBitmap& bitmap) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  void FlushForTesting();

  void set_media_controller_for_testing(
      media_session::mojom::MediaControllerPtr controller) {
    media_controller_ptr_ = std::move(controller);
  }

  void set_timer_for_testing(std::unique_ptr<base::OneShotTimer> test_timer) {
    hide_controls_timer_ = std::move(test_timer);
  }

 private:
  friend class LockScreenMediaControlsViewTest;

  // Sets the media artwork to |img|. If |img| is nullopt, the default artwork
  // is set instead.
  void SetArtwork(base::Optional<gfx::ImageSkia> img);

  // Creates and adds a new media button to |button_row_|. This should not be
  // used to create toggle buttons such as play/pause.
  void CreateMediaButton(int size,
                         media_session::mojom::MediaSessionAction action,
                         const base::string16& accessible_name);

  // Updates the visibility of buttons on |button_row_| depending on what is
  // available in the current media session.
  void UpdateActionButtonsVisibility();

  // Toggles the media play/pause button between "play" and "pause" as
  // necessary.
  void SetIsPlaying(bool playing);

  // Used to connect to the Media Session service.
  service_manager::Connector* const connector_;

  // Used to control the active session.
  media_session::mojom::MediaControllerPtr media_controller_ptr_;

  // Used to receive updates to the active media controller.
  mojo::Receiver<media_session::mojom::MediaControllerObserver>
      observer_receiver_{this};

  // Used to receive updates to the active media's icon.
  mojo::Receiver<media_session::mojom::MediaControllerImageObserver>
      icon_observer_receiver_{this};

  // Used to receive updates to the active media's artwork.
  mojo::Receiver<media_session::mojom::MediaControllerImageObserver>
      artwork_observer_receiver_{this};

  // The id of the current media session. It will be null if there is not
  // a current session.
  base::Optional<base::UnguessableToken> media_session_id_;

  // Spacing between controls and user.
  std::unique_ptr<views::View> middle_spacing_;

  // Automatically hides the controls a few seconds if no media playing.
  std::unique_ptr<base::OneShotTimer> hide_controls_timer_;

  // Caches the text to be read by screen readers describing the media controls
  // view.
  base::string16 accessible_name_;

  // Set of enabled actions.
  std::set<media_session::mojom::MediaSessionAction> enabled_actions_;

  // Container views directly attached to this view.
  MediaControlsHeaderView* header_row_ = nullptr;
  views::ImageView* session_artwork_ = nullptr;
  NonAccessibleView* button_row_ = nullptr;
  views::ToggleImageButton* play_pause_button_ = nullptr;
  views::ImageButton* close_button_ = nullptr;

  // Callbacks.
  const MediaControlsEnabled media_controls_enabled_;
  const base::RepeatingClosure hide_media_controls_;
  const base::RepeatingClosure show_media_controls_;

  DISALLOW_COPY_AND_ASSIGN(LockScreenMediaControlsView);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOCK_SCREEN_MEDIA_CONTROLS_VIEW_H_
