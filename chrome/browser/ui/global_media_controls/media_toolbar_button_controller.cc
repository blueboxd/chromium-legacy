// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller.h"

#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller_delegate.h"
#include "services/media_session/public/mojom/constants.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

MediaToolbarButtonController::MediaToolbarButtonController(
    service_manager::Connector* connector,
    MediaToolbarButtonControllerDelegate* delegate)
    : connector_(connector), delegate_(delegate) {
  DCHECK(delegate_);

  // |connector| can be null in tests.
  if (!connector_)
    return;

  // Connect to the MediaControllerManager and create a MediaController that
  // controls the active session so we can observe it.
  media_session::mojom::MediaControllerManagerPtr controller_manager_ptr;
  connector_->BindInterface(media_session::mojom::kServiceName,
                            mojo::MakeRequest(&controller_manager_ptr));
  controller_manager_ptr->CreateActiveMediaController(
      mojo::MakeRequest(&media_controller_ptr_));

  // Observe the active media controller for changes to playback state and
  // supported actions.
  media_controller_ptr_->AddObserver(
      media_controller_observer_receiver_.BindNewPipeAndPassRemote());
}

MediaToolbarButtonController::~MediaToolbarButtonController() = default;

void MediaToolbarButtonController::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr session_info) {
  if (session_info && session_info->is_controllable)
    delegate_->Show();
}
