// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/audio_focus_request.h"

#include "services/media_session/audio_focus_manager.h"

namespace media_session {

AudioFocusRequest::AudioFocusRequest(
    base::WeakPtr<AudioFocusManager> owner,
    mojom::AudioFocusRequestClientRequest request,
    mojom::MediaSessionPtr session,
    mojom::MediaSessionInfoPtr session_info,
    mojom::AudioFocusType audio_focus_type,
    const base::UnguessableToken& id,
    const std::string& source_name,
    const base::UnguessableToken& group_id)
    : metrics_helper_(source_name),
      session_(std::move(session)),
      session_info_(std::move(session_info)),
      audio_focus_type_(audio_focus_type),
      binding_(this, std::move(request)),
      id_(id),
      source_name_(source_name),
      group_id_(group_id),
      owner_(std::move(owner)) {
  // Listen for mojo errors.
  binding_.set_connection_error_handler(base::BindOnce(
      &AudioFocusRequest::OnConnectionError, base::Unretained(this)));
  session_.set_connection_error_handler(base::BindOnce(
      &AudioFocusRequest::OnConnectionError, base::Unretained(this)));

  metrics_helper_.OnRequestAudioFocus(
      AudioFocusManagerMetricsHelper::AudioFocusRequestSource::kInitial,
      audio_focus_type);
}

AudioFocusRequest::~AudioFocusRequest() = default;

void AudioFocusRequest::RequestAudioFocus(
    mojom::MediaSessionInfoPtr session_info,
    mojom::AudioFocusType type,
    RequestAudioFocusCallback callback) {
  SetSessionInfo(std::move(session_info));

  if (session_info_->state == mojom::MediaSessionInfo::SessionState::kActive &&
      owner_->IsSessionOnTopOfAudioFocusStack(id(), type)) {
    // Return early is this session is already on top of the stack.
    std::move(callback).Run();
    return;
  }

  // Remove this AudioFocusRequest for the audio focus stack.
  std::unique_ptr<AudioFocusRequest> row =
      owner_->RemoveFocusEntryIfPresent(id());
  DCHECK(row);

  owner_->RequestAudioFocusInternal(std::move(row), type);

  std::move(callback).Run();

  metrics_helper_.OnRequestAudioFocus(
      AudioFocusManagerMetricsHelper::AudioFocusRequestSource::kUpdate,
      audio_focus_type_);
}

void AudioFocusRequest::AbandonAudioFocus() {
  metrics_helper_.OnAbandonAudioFocus(
      AudioFocusManagerMetricsHelper::AudioFocusAbandonSource::kAPI);

  owner_->AbandonAudioFocusInternal(id_);
}

void AudioFocusRequest::MediaSessionInfoChanged(
    mojom::MediaSessionInfoPtr info) {
  bool suspended_change =
      (info->state == mojom::MediaSessionInfo::SessionState::kSuspended ||
       IsSuspended()) &&
      info->state != session_info_->state;

  SetSessionInfo(std::move(info));

  // If we have transitioned to/from a suspended state then we should
  // re-enforce audio focus.
  if (suspended_change)
    owner_->EnforceAudioFocus();
}

bool AudioFocusRequest::IsSuspended() const {
  return session_info_->state ==
         mojom::MediaSessionInfo::SessionState::kSuspended;
}

mojom::AudioFocusRequestStatePtr AudioFocusRequest::ToAudioFocusRequestState()
    const {
  auto request = mojom::AudioFocusRequestState::New();
  request->session_info = session_info_.Clone();
  request->audio_focus_type = audio_focus_type_;
  request->request_id = id_;
  request->source_name = source_name_;
  return request;
}

void AudioFocusRequest::BindToMediaController(
    mojom::MediaControllerRequest request) {
  if (!controller_) {
    controller_ = std::make_unique<MediaController>();
    controller_->SetMediaSession(this);
  }

  controller_->BindToInterface(std::move(request));
}

void AudioFocusRequest::Suspend(const EnforcementState& state) {
  DCHECK(!session_info_->force_duck);

  // In most cases if we stop or suspend we should call the ::Suspend method
  // on the media session. The only exception is if the session has the
  // |prefer_stop_for_gain_focus_loss| bit set in which case we should use
  // ::Stop and ::Suspend respectively.
  if (state.should_stop && session_info_->prefer_stop_for_gain_focus_loss) {
    session_->Stop(mojom::MediaSession::SuspendType::kSystem);
  } else {
    was_suspended_ = was_suspended_ || state.should_suspend;
    session_->Suspend(mojom::MediaSession::SuspendType::kSystem);
  }
}

void AudioFocusRequest::MaybeResume() {
  DCHECK(!session_info_->force_duck);

  if (!was_suspended_)
    return;

  was_suspended_ = false;
  session_->Resume(mojom::MediaSession::SuspendType::kSystem);
}

void AudioFocusRequest::SetSessionInfo(
    mojom::MediaSessionInfoPtr session_info) {
  bool is_controllable_changed =
      session_info_->is_controllable != session_info->is_controllable;

  session_info_ = std::move(session_info);

  if (is_controllable_changed)
    owner_->MaybeUpdateActiveSession();
}

void AudioFocusRequest::OnConnectionError() {
  // Since we have multiple pathways that can call |OnConnectionError| we
  // should use the |encountered_error_| bit to make sure we abandon focus
  // just the first time.
  if (encountered_error_)
    return;

  encountered_error_ = true;

  metrics_helper_.OnAbandonAudioFocus(
      AudioFocusManagerMetricsHelper::AudioFocusAbandonSource::
          kConnectionError);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&AudioFocusManager::AbandonAudioFocusInternal,
                                owner_, id_));
}

}  // namespace media_session
