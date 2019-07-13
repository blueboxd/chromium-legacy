// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOCK_SCREEN_MEDIA_CONTROLS_VIEW_H_
#define ASH_LOGIN_UI_LOCK_SCREEN_MEDIA_CONTROLS_VIEW_H_

#include "ash/ash_export.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "ui/views/view.h"

namespace service_manager {
class Connector;
}

namespace views {
class ImageView;
}

namespace ash {

class LockContentsView;
class MediaControlsHeaderView;

class ASH_EXPORT LockScreenMediaControlsView
    : public views::View,
      public media_session::mojom::MediaControllerObserver,
      public media_session::mojom::MediaControllerImageObserver {
 public:
  LockScreenMediaControlsView(service_manager::Connector* connector,
                              LockContentsView* view);
  ~LockScreenMediaControlsView() override;

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  views::View* GetMiddleSpacingView();

  // media_session::mojom::MediaControllerObserver:
  void MediaSessionInfoChanged(
      media_session::mojom::MediaSessionInfoPtr session_info) override;
  void MediaSessionMetadataChanged(
      const base::Optional<media_session::MediaMetadata>& metadata) override;
  void MediaSessionActionsChanged(
      const std::vector<media_session::mojom::MediaSessionAction>& actions)
      override {}
  void MediaSessionChanged(
      const base::Optional<base::UnguessableToken>& request_id) override {}

  // media_session::mojom::MediaControllerImageObserver:
  void MediaControllerImageChanged(
      media_session::mojom::MediaSessionImageType type,
      const SkBitmap& bitmap) override;

  void set_timer_for_testing(std::unique_ptr<base::OneShotTimer> test_timer) {
    hide_controls_timer_ = std::move(test_timer);
  }

 private:
  friend class LockScreenMediaControlsViewTest;

  // Sets the media artwork to |img|. If |img| is nullopt, the default artwork
  // is set instead.
  void SetArtwork(base::Optional<gfx::ImageSkia> img);

  // Lock screen view which this view belongs to.
  LockContentsView* const view_;

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

  // The info about the current media session. It will be null if there is not
  // a current session.
  media_session::mojom::MediaSessionInfoPtr media_session_info_;

  // Spacing between controls and user.
  std::unique_ptr<views::View> middle_spacing_;

  // Automatically hides the controls a few seconds if no media playing.
  std::unique_ptr<base::OneShotTimer> hide_controls_timer_;

  // Caches the text to be read by screen readers describing the media controls
  // view.
  base::string16 accessible_name_;

  // Container views directly attached to this view.
  MediaControlsHeaderView* header_row_ = nullptr;
  views::ImageView* session_artwork_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(LockScreenMediaControlsView);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOCK_SCREEN_MEDIA_CONTROLS_VIEW_H_
