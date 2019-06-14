// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/remote_command_media_keys_listener_mac.h"

#include "base/mac/mac_util.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/now_playing/remote_command_center_delegate.h"

namespace ui {

// static
bool RemoteCommandMediaKeysListenerMac::has_instance_ = false;

RemoteCommandMediaKeysListenerMac::RemoteCommandMediaKeysListenerMac(
    MediaKeysListener::Delegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
  DCHECK(!has_instance_);
  has_instance_ = true;
}

RemoteCommandMediaKeysListenerMac::~RemoteCommandMediaKeysListenerMac() {
  has_instance_ = false;
}

void RemoteCommandMediaKeysListenerMac::Initialize() {
  if (!remote_command_center_delegate_) {
    remote_command_center_delegate_ =
        now_playing::RemoteCommandCenterDelegate::GetInstance();
  }
  DCHECK(remote_command_center_delegate_);

  remote_command_center_delegate_->AddObserver(this);
}

bool RemoteCommandMediaKeysListenerMac::StartWatchingMediaKey(
    KeyboardCode key_code) {
  DCHECK(IsMediaKeycode(key_code));
  DCHECK(remote_command_center_delegate_);

  key_codes_.insert(key_code);

  switch (key_code) {
    case VKEY_MEDIA_PLAY_PAUSE:
      remote_command_center_delegate_->SetCanPlay(true);
      remote_command_center_delegate_->SetCanPause(true);
      remote_command_center_delegate_->SetCanPlayPause(true);
      break;
    case VKEY_MEDIA_STOP:
      remote_command_center_delegate_->SetCanStop(true);
      break;
    case VKEY_MEDIA_NEXT_TRACK:
      remote_command_center_delegate_->SetCanGoNextTrack(true);
      break;
    case VKEY_MEDIA_PREV_TRACK:
      remote_command_center_delegate_->SetCanGoPreviousTrack(true);
      break;
    default:
      NOTREACHED();
  }

  return true;
}

void RemoteCommandMediaKeysListenerMac::StopWatchingMediaKey(
    KeyboardCode key_code) {
  DCHECK(IsMediaKeycode(key_code));
  DCHECK(remote_command_center_delegate_);

  key_codes_.erase(key_code);

  switch (key_code) {
    case VKEY_MEDIA_PLAY_PAUSE:
      remote_command_center_delegate_->SetCanPlay(false);
      remote_command_center_delegate_->SetCanPause(false);
      remote_command_center_delegate_->SetCanPlayPause(false);
      break;
    case VKEY_MEDIA_STOP:
      remote_command_center_delegate_->SetCanStop(false);
      break;
    case VKEY_MEDIA_NEXT_TRACK:
      remote_command_center_delegate_->SetCanGoNextTrack(false);
      break;
    case VKEY_MEDIA_PREV_TRACK:
      remote_command_center_delegate_->SetCanGoPreviousTrack(false);
      break;
    default:
      NOTREACHED();
  }
}

void RemoteCommandMediaKeysListenerMac::SetIsMediaPlaying(bool is_playing) {
  is_media_playing_ = is_playing;
}

void RemoteCommandMediaKeysListenerMac::OnNext() {
  MaybeSend(VKEY_MEDIA_NEXT_TRACK);
}

void RemoteCommandMediaKeysListenerMac::OnPrevious() {
  MaybeSend(VKEY_MEDIA_PREV_TRACK);
}

void RemoteCommandMediaKeysListenerMac::OnPause() {
  if (is_media_playing_)
    MaybeSend(VKEY_MEDIA_PLAY_PAUSE);
}

void RemoteCommandMediaKeysListenerMac::OnPlayPause() {
  MaybeSend(VKEY_MEDIA_PLAY_PAUSE);
}

void RemoteCommandMediaKeysListenerMac::OnStop() {
  MaybeSend(VKEY_MEDIA_STOP);
}

void RemoteCommandMediaKeysListenerMac::OnPlay() {
  if (!is_media_playing_)
    MaybeSend(VKEY_MEDIA_PLAY_PAUSE);
}

void RemoteCommandMediaKeysListenerMac::MaybeSend(KeyboardCode key_code) {
  if (!key_codes_.contains(key_code))
    return;

  delegate_->OnMediaKeysAccelerator(Accelerator(key_code, /*modifiers=*/0));
}

}  // namespace ui
