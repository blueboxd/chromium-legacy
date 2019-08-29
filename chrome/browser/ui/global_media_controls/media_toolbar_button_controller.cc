// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller.h"

#include "chrome/browser/ui/global_media_controls/media_dialog_delegate.h"
#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller_delegate.h"
#include "components/media_message_center/media_notification_item.h"
#include "components/media_message_center/media_notification_util.h"
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

  // Connect to the controller manager so we can create media controllers for
  // media sessions.
  connector_->Connect(media_session::mojom::kServiceName,
                      controller_manager_remote_.BindNewPipeAndPassReceiver());

  // Connect to receive audio focus events.
  connector_->Connect(media_session::mojom::kServiceName,
                      audio_focus_remote_.BindNewPipeAndPassReceiver());
  audio_focus_remote_->AddObserver(
      audio_focus_observer_receiver_.BindNewPipeAndPassRemote());

  audio_focus_remote_->GetFocusRequests(base::BindOnce(
      &MediaToolbarButtonController::OnReceivedAudioFocusRequests,
      weak_ptr_factory_.GetWeakPtr()));
}

MediaToolbarButtonController::~MediaToolbarButtonController() = default;

void MediaToolbarButtonController::OnFocusGained(
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
    active_controllable_session_ids_.insert(id);
    frozen_session_ids_.erase(id);
    UpdateToolbarButtonState();
  } else {
    sessions_.emplace(
        std::piecewise_construct, std::forward_as_tuple(id),
        std::forward_as_tuple(
            this, id, session->source_name.value_or(std::string()),
            std::move(controller), std::move(session->session_info)));
  }
}

void MediaToolbarButtonController::OnFocusLost(
    media_session::mojom::AudioFocusRequestStatePtr session) {
  const std::string id = session->request_id->ToString();

  auto it = sessions_.find(id);
  if (it == sessions_.end())
    return;

  it->second.Freeze();
  active_controllable_session_ids_.erase(id);
  frozen_session_ids_.insert(id);
  UpdateToolbarButtonState();
}

void MediaToolbarButtonController::ShowNotification(const std::string& id) {
  active_controllable_session_ids_.insert(id);
  UpdateToolbarButtonState();

  if (!dialog_delegate_)
    return;

  base::WeakPtr<media_message_center::MediaNotificationItem> item;

  auto it = sessions_.find(id);
  if (it != sessions_.end())
    item = it->second.GetWeakPtr();

  dialog_delegate_->ShowMediaSession(id, item);
}

void MediaToolbarButtonController::HideNotification(const std::string& id) {
  active_controllable_session_ids_.erase(id);
  frozen_session_ids_.erase(id);
  UpdateToolbarButtonState();

  if (!dialog_delegate_)
    return;

  dialog_delegate_->HideMediaSession(id);
}

scoped_refptr<base::SequencedTaskRunner>
MediaToolbarButtonController::GetTaskRunner() const {
  return nullptr;
}

void MediaToolbarButtonController::RemoveItem(const std::string& id) {
  active_controllable_session_ids_.erase(id);
  frozen_session_ids_.erase(id);
  sessions_.erase(id);

  UpdateToolbarButtonState();
}

void MediaToolbarButtonController::SetDialogDelegate(
    MediaDialogDelegate* delegate) {
  DCHECK(!delegate || !dialog_delegate_);
  dialog_delegate_ = delegate;

  UpdateToolbarButtonState();

  if (!dialog_delegate_)
    return;

  for (const std::string& id : active_controllable_session_ids_) {
    base::WeakPtr<media_message_center::MediaNotificationItem> item;

    auto it = sessions_.find(id);
    if (it != sessions_.end())
      item = it->second.GetWeakPtr();

    dialog_delegate_->ShowMediaSession(id, item);
  }

  media_message_center::RecordConcurrentNotificationCount(
      active_controllable_session_ids_.size());
}

void MediaToolbarButtonController::OnReceivedAudioFocusRequests(
    std::vector<media_session::mojom::AudioFocusRequestStatePtr> sessions) {
  for (auto& session : sessions)
    OnFocusGained(std::move(session));
}

void MediaToolbarButtonController::UpdateToolbarButtonState() {
  if (!active_controllable_session_ids_.empty()) {
    if (delegate_display_state_ != DisplayState::kShown) {
      delegate_->Enable();
      delegate_->Show();
    }
    delegate_display_state_ = DisplayState::kShown;
    return;
  }

  if (frozen_session_ids_.empty()) {
    if (delegate_display_state_ != DisplayState::kHidden)
      delegate_->Hide();
    delegate_display_state_ = DisplayState::kHidden;
    return;
  }

  if (!dialog_delegate_) {
    if (delegate_display_state_ != DisplayState::kDisabled)
      delegate_->Disable();
    delegate_display_state_ = DisplayState::kDisabled;
  }
}
