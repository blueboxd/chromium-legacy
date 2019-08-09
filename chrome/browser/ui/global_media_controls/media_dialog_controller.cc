// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/media_dialog_controller.h"

#include "base/bind.h"
#include "base/containers/adapters.h"
#include "chrome/browser/ui/global_media_controls/media_dialog_controller_delegate.h"
#include "components/media_message_center/media_notification_item.h"
#include "services/media_session/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

using media_session::mojom::MediaSessionAction;

MediaDialogController::MediaDialogController(
    service_manager::Connector* connector,
    MediaDialogControllerDelegate* delegate)
    : connector_(connector), delegate_(delegate) {
  DCHECK(delegate_);
}

MediaDialogController::~MediaDialogController() = default;

void MediaDialogController::Initialize() {
  // |connector| can be null in tests.
  if (!connector_)
    return;

  // Connect to the controller manager so we can create media controllers for
  // media sessions.
  connector_->Connect(media_session::mojom::kServiceName,
                      controller_manager_remote_.BindNewPipeAndPassReceiver());

  // Connect to receive audio focus events.
  connector_->Connect(media_session::mojom::kServiceName,
                      audio_focus_remote_.BindNewPipeAndPassReceiver());
  audio_focus_remote_->AddObserver(
      audio_focus_observer_receiver_.BindNewPipeAndPassRemote());

  audio_focus_remote_->GetFocusRequests(
      base::BindOnce(&MediaDialogController::OnReceivedAudioFocusRequests,
                     weak_ptr_factory_.GetWeakPtr()));
}

void MediaDialogController::OnFocusGained(
    media_session::mojom::AudioFocusRequestStatePtr session) {
  const std::string id = session->request_id->ToString();

  // If we have an existing unfrozen item then this is a duplicate call and
  // we should ignore it.
  auto it = sessions_.find(id);
  if (it != sessions_.end() && !it->second.frozen())
    return;

  mojo::Remote<media_session::mojom::MediaController> controller;

  // |controller_manager_remote_| may be null in tests where connector is
  // unavailable.
  if (controller_manager_remote_) {
    controller_manager_remote_->CreateMediaControllerForSession(
        controller.BindNewPipeAndPassReceiver(), *session->request_id);
  }

  if (it != sessions_.end()) {
    // If the notification was previously frozen then we should reset the
    // controller because the mojo pipe would have been reset.
    it->second.SetController(std::move(controller),
                             std::move(session->session_info));
  } else {
    sessions_.emplace(
        std::piecewise_construct, std::forward_as_tuple(id),
        std::forward_as_tuple(
            this, id, session->source_name.value_or(std::string()),
            std::move(controller), std::move(session->session_info)));
  }
}

void MediaDialogController::OnFocusLost(
    media_session::mojom::AudioFocusRequestStatePtr session) {
  auto it = sessions_.find(session->request_id->ToString());
  if (it == sessions_.end())
    return;

  it->second.Freeze();
}

void MediaDialogController::ShowNotification(const std::string& id) {
  base::WeakPtr<media_message_center::MediaNotificationItem> item;

  auto it = sessions_.find(id);
  if (it != sessions_.end())
    item = it->second.GetWeakPtr();

  delegate_->ShowMediaSession(id, item);
}

void MediaDialogController::HideNotification(const std::string& id) {
  delegate_->HideMediaSession(id);
}

void MediaDialogController::RemoveItem(const std::string& id) {
  sessions_.erase(id);
}

scoped_refptr<base::SequencedTaskRunner> MediaDialogController::GetTaskRunner()
    const {
  return task_runner_for_testing_;
}

void MediaDialogController::OnReceivedAudioFocusRequests(
    std::vector<media_session::mojom::AudioFocusRequestStatePtr> sessions) {
  for (auto& session : sessions)
    OnFocusGained(std::move(session));
}
