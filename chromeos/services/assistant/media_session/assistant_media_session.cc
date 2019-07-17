// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/media_session/assistant_media_session.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/services/assistant/assistant_manager_service_impl.h"
#include "services/media_session/public/cpp/features.h"
#include "services/media_session/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace chromeos {
namespace assistant {

namespace {

using media_session::mojom::AudioFocusType;
using media_session::mojom::MediaPlaybackState;
using media_session::mojom::MediaSessionInfo;

const char kAudioFocusSourceName[] = "assistant";

}  // namespace

AssistantMediaSession::AssistantMediaSession(
    service_manager::Connector* connector,
    AssistantManagerServiceImpl* assistant_manager)
    : assistant_manager_service_(assistant_manager),
      connector_(connector),
      binding_(this),
      weak_factory_(this) {}

AssistantMediaSession::~AssistantMediaSession() {
  AbandonAudioFocusIfNeeded();
}

void AssistantMediaSession::GetMediaSessionInfo(
    GetMediaSessionInfoCallback callback) {
  std::move(callback).Run(GetMediaSessionInfoInternal());
}

void AssistantMediaSession::AddObserver(
    mojo::PendingRemote<media_session::mojom::MediaSessionObserver> observer) {
  mojo::Remote<media_session::mojom::MediaSessionObserver>
      media_session_observer(std::move(observer));
  media_session_observer->MediaSessionInfoChanged(
      GetMediaSessionInfoInternal());
  media_session_observer->MediaSessionMetadataChanged(metadata_);
  observers_.Add(std::move(media_session_observer));
}

void AssistantMediaSession::GetDebugInfo(GetDebugInfoCallback callback) {
  media_session::mojom::MediaSessionDebugInfoPtr info(
      media_session::mojom::MediaSessionDebugInfo::New());
  std::move(callback).Run(std::move(info));
}

// TODO(b/135064564): Update StartDucking() and StopDucking() after volume
// control API for media streams is implemented.
void AssistantMediaSession::StartDucking() {
  if (is_ducking_)
    return;
  is_ducking_ = true;
  Suspend(SuspendType::kSystem);
}

void AssistantMediaSession::StopDucking() {
  if (!is_ducking_)
    return;
  is_ducking_ = false;
  Resume(SuspendType::kSystem);
}

void AssistantMediaSession::Suspend(SuspendType suspend_type) {
  if (!IsActive())
    return;

  SetAudioFocusInfo(State::SUSPENDED, audio_focus_type_);
  assistant_manager_service_->UpdateInternalMediaPlayerStatus(
      media_session::mojom::MediaSessionAction::kPause);
}

void AssistantMediaSession::Resume(SuspendType suspend_type) {
  if (!IsSuspended())
    return;

  SetAudioFocusInfo(State::ACTIVE, audio_focus_type_);
  assistant_manager_service_->UpdateInternalMediaPlayerStatus(
      media_session::mojom::MediaSessionAction::kPlay);
}

void AssistantMediaSession::RequestAudioFocus(AudioFocusType audio_focus_type) {
  if (!base::FeatureList::IsEnabled(
          media_session::features::kMediaSessionService)) {
    return;
  }

  if (request_client_ptr_.is_bound()) {
    // We have an existing request so we should request an updated focus type.
    request_client_ptr_->RequestAudioFocus(
        GetMediaSessionInfoInternal(), audio_focus_type,
        base::BindOnce(&AssistantMediaSession::FinishAudioFocusRequest,
                       base::Unretained(this), audio_focus_type));
    return;
  }

  EnsureServiceConnection();

  // Create a mojo interface pointer to our media session.
  media_session::mojom::MediaSessionPtr media_session;
  binding_.Close();
  binding_.Bind(mojo::MakeRequest(&media_session));
  audio_focus_ptr_->RequestAudioFocus(
      mojo::MakeRequest(&request_client_ptr_), std::move(media_session),
      GetMediaSessionInfoInternal(), audio_focus_type,
      base::BindOnce(&AssistantMediaSession::FinishInitialAudioFocusRequest,
                     base::Unretained(this), audio_focus_type));
}

void AssistantMediaSession::AbandonAudioFocusIfNeeded() {
  if (!base::FeatureList::IsEnabled(
          media_session::features::kMediaSessionService)) {
    return;
  }

  if (audio_focus_state_ == State::INACTIVE)
    return;

  SetAudioFocusInfo(State::INACTIVE, audio_focus_type_);

  if (!request_client_ptr_.is_bound())
    return;

  request_client_ptr_->AbandonAudioFocus();
  request_client_ptr_.reset();
  audio_focus_ptr_.reset();
}

void AssistantMediaSession::EnsureServiceConnection() {
  DCHECK(base::FeatureList::IsEnabled(
      media_session::features::kMediaSessionService));

  if (audio_focus_ptr_.is_bound() && !audio_focus_ptr_.encountered_error())
    return;

  audio_focus_ptr_.reset();
  connector_->BindInterface(media_session::mojom::kServiceName,
                            mojo::MakeRequest(&audio_focus_ptr_));
  audio_focus_ptr_->SetSourceName(kAudioFocusSourceName);
}

void AssistantMediaSession::FinishAudioFocusRequest(
    AudioFocusType audio_focus_type) {
  DCHECK(request_client_ptr_.is_bound());

  SetAudioFocusInfo(State::ACTIVE, audio_focus_type);
}

void AssistantMediaSession::FinishInitialAudioFocusRequest(
    AudioFocusType audio_focus_type,
    const base::UnguessableToken& request_id) {
  FinishAudioFocusRequest(audio_focus_type);
}

void AssistantMediaSession::SetAudioFocusInfo(State audio_focus_state,
                                              AudioFocusType audio_focus_type) {
  if (audio_focus_state == audio_focus_state_ &&
      audio_focus_type == audio_focus_type_) {
    return;
  }

  audio_focus_state_ = audio_focus_state;
  audio_focus_type_ = audio_focus_type;
  NotifyMediaSessionInfoChanged();
}

void AssistantMediaSession::NotifyMediaSessionMetadataChanged(
    const assistant_client::MediaStatus& status) {
  media_session::MediaMetadata metadata;

  metadata.title = base::UTF8ToUTF16(status.metadata.title);
  metadata.artist = base::UTF8ToUTF16(status.metadata.artist);
  metadata.album = base::UTF8ToUTF16(status.metadata.album);

  bool metadata_changed = metadata_ != metadata;
  if (!metadata_changed)
    return;

  metadata_ = metadata;

  current_track_ = status.track_type;

  for (auto& observer : observers_)
    observer->MediaSessionMetadataChanged(this->metadata_);
}

media_session::mojom::MediaSessionInfoPtr
AssistantMediaSession::GetMediaSessionInfoInternal() {
  media_session::mojom::MediaSessionInfoPtr info(
      media_session::mojom::MediaSessionInfo::New());
  switch (audio_focus_state_) {
    case State::ACTIVE:
      info->state = MediaSessionInfo::SessionState::kActive;
      break;
    case State::SUSPENDED:
      info->state = MediaSessionInfo::SessionState::kSuspended;
      break;
    case State::INACTIVE:
      info->state = MediaSessionInfo::SessionState::kInactive;
      break;
  }
  if (audio_focus_state_ != State::INACTIVE &&
      audio_focus_type_ != AudioFocusType::kGainTransient) {
    info->is_controllable = true;
  }
  return info;
}

void AssistantMediaSession::NotifyMediaSessionInfoChanged() {
  media_session::mojom::MediaSessionInfoPtr current_info =
      GetMediaSessionInfoInternal();

  if (current_info == session_info_)
    return;

  if (request_client_ptr_.is_bound())
    request_client_ptr_->MediaSessionInfoChanged(current_info.Clone());

  for (auto& observer : observers_)
    observer->MediaSessionInfoChanged(current_info.Clone());

  session_info_ = std::move(current_info);
}

bool AssistantMediaSession::IsActive() const {
  return audio_focus_state_ == State::ACTIVE;
}

bool AssistantMediaSession::IsSuspended() const {
  return audio_focus_state_ == State::SUSPENDED;
}

base::WeakPtr<AssistantMediaSession> AssistantMediaSession::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace assistant
}  // namespace chromeos
