// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_MEDIA_STRING_VIEW_H_
#define ASH_AMBIENT_UI_MEDIA_STRING_VIEW_H_

#include <string>
#include "ash/app_menu/notification_item_view.h"
#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"

namespace ash {

// Container for displaying ongoing media information, including the name of the
// media and the artist, formatted with a proceding music note symbol and a
// middle dot separator.
class MediaStringView : public views::Label,
                        public media_session::mojom::MediaControllerObserver {
 public:
  MediaStringView();
  MediaStringView(const MediaStringView&) = delete;
  MediaStringView& operator=(const MediaStringView&) = delete;
  ~MediaStringView() override;

  // views::Label:
  const char* GetClassName() const override;

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
  void MediaSessionPositionChanged(
      const base::Optional<media_session::MediaPosition>& position) override {}

 private:
  void InitLayout();

  void BindMediaControllerObserver();

  // Used to receive updates to the active media controller.
  mojo::Remote<media_session::mojom::MediaController> media_controller_remote_;
  mojo::Receiver<media_session::mojom::MediaControllerObserver>
      observer_receiver_{this};
};

}  // namespace ash

#endif  // ASH_AMBIENT_UI_MEDIA_STRING_VIEW_H_
