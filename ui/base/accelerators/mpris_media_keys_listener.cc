// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/mpris_media_keys_listener.h"

#include "ui/base/accelerators/accelerator.h"
#include "ui/base/mpris/mpris_service.h"

namespace ui {

// static
bool MprisMediaKeysListener::has_instance_ = false;

MprisMediaKeysListener::MprisMediaKeysListener(
    MediaKeysListener::Delegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
  DCHECK(!has_instance_);
  has_instance_ = true;
}

MprisMediaKeysListener::~MprisMediaKeysListener() {
  DCHECK(has_instance_);
  has_instance_ = false;
}

void MprisMediaKeysListener::Initialize() {
  // |service_| can be set for tests.
  if (!service_)
    service_ = mpris::MprisService::GetInstance();
  DCHECK(service_);

  service_->AddObserver(this);
}

bool MprisMediaKeysListener::StartWatchingMediaKey(KeyboardCode key_code) {
  DCHECK(IsMediaKeycode(key_code));

  key_codes_.insert(key_code);

  DCHECK(service_);

  switch (key_code) {
    case VKEY_MEDIA_PLAY_PAUSE:
      service_->SetCanPlay(true);
      service_->SetCanPause(true);
      break;
    case VKEY_MEDIA_NEXT_TRACK:
      service_->SetCanGoNext(true);
      break;
    case VKEY_MEDIA_PREV_TRACK:
      service_->SetCanGoPrevious(true);
      break;
    case VKEY_MEDIA_STOP:
      // No properties need to be changed.
      break;
    default:
      NOTREACHED();
  }

  return true;
}

void MprisMediaKeysListener::StopWatchingMediaKey(KeyboardCode key_code) {
  DCHECK(IsMediaKeycode(key_code));

  key_codes_.erase(key_code);

  DCHECK(service_);

  switch (key_code) {
    case VKEY_MEDIA_PLAY_PAUSE:
      service_->SetCanPlay(false);
      service_->SetCanPause(false);
      break;
    case VKEY_MEDIA_NEXT_TRACK:
      service_->SetCanGoNext(false);
      break;
    case VKEY_MEDIA_PREV_TRACK:
      service_->SetCanGoPrevious(false);
      break;
    case VKEY_MEDIA_STOP:
      // No properties need to be changed.
      break;
    default:
      NOTREACHED();
  }
}

void MprisMediaKeysListener::SetIsMediaPlaying(bool is_playing) {
  is_media_playing_ = is_playing;
}

void MprisMediaKeysListener::OnNext() {
  MaybeSendKeyCode(VKEY_MEDIA_NEXT_TRACK);
}

void MprisMediaKeysListener::OnPrevious() {
  MaybeSendKeyCode(VKEY_MEDIA_PREV_TRACK);
}

void MprisMediaKeysListener::OnPause() {
  if (is_media_playing_)
    MaybeSendKeyCode(VKEY_MEDIA_PLAY_PAUSE);
}

void MprisMediaKeysListener::OnPlayPause() {
  MaybeSendKeyCode(VKEY_MEDIA_PLAY_PAUSE);
}

void MprisMediaKeysListener::OnStop() {
  MaybeSendKeyCode(VKEY_MEDIA_STOP);
}

void MprisMediaKeysListener::OnPlay() {
  if (!is_media_playing_)
    MaybeSendKeyCode(VKEY_MEDIA_PLAY_PAUSE);
}

void MprisMediaKeysListener::MaybeSendKeyCode(KeyboardCode key_code) {
  if (!key_codes_.contains(key_code))
    return;

  Accelerator accelerator(key_code, /*modifiers=*/0);
  delegate_->OnMediaKeysAccelerator(accelerator);
}

}  // namespace ui
