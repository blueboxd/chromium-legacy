// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/audio_session_event_listener_win.h"

namespace media {

AudioSessionEventListener::AudioSessionEventListener(
    IAudioClient* client,
    base::OnceClosure device_change_cb)
    : device_change_cb_(std::move(device_change_cb)) {
  DCHECK(device_change_cb_);

  HRESULT hr = client->GetService(IID_PPV_ARGS(&audio_session_control_));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get IAudioSessionControl service.";
    return;
  }

  audio_session_control_->RegisterAudioSessionNotification(this);
}

AudioSessionEventListener::~AudioSessionEventListener() {
  if (!audio_session_control_)
    return;

  HRESULT hr = audio_session_control_->UnregisterAudioSessionNotification(this);
  DLOG_IF(ERROR, FAILED(hr))
      << "UnregisterAudioSessionNotification() failed: " << std::hex << hr;
}

STDMETHODIMP_(ULONG) AudioSessionEventListener::AddRef() {
  return 1;  // Class is owned in Chromium code and should have no outside refs.
}

STDMETHODIMP_(ULONG) AudioSessionEventListener::Release() {
  return 1;  // Class is owned in Chromium code and should have no outside refs.
}

STDMETHODIMP AudioSessionEventListener::QueryInterface(REFIID iid,
                                                       void** object) {
  if (iid == IID_IUnknown || iid == __uuidof(IAudioSessionEvents)) {
    *object = static_cast<IAudioSessionEvents*>(this);
    return S_OK;
  }

  *object = nullptr;
  return E_NOINTERFACE;
}

STDMETHODIMP AudioSessionEventListener::OnChannelVolumeChanged(
    DWORD channel_count,
    float new_channel_volume_array[],
    DWORD changed_channel,
    LPCGUID event_context) {
  return S_OK;
}

STDMETHODIMP
AudioSessionEventListener::OnDisplayNameChanged(LPCWSTR new_display_name,
                                                LPCGUID event_context) {
  return S_OK;
}

STDMETHODIMP AudioSessionEventListener::OnGroupingParamChanged(
    LPCGUID new_grouping_param,
    LPCGUID event_context) {
  return S_OK;
}

STDMETHODIMP AudioSessionEventListener::OnIconPathChanged(
    LPCWSTR new_icon_path,
    LPCGUID event_context) {
  return S_OK;
}

STDMETHODIMP AudioSessionEventListener::OnSessionDisconnected(
    AudioSessionDisconnectReason disconnect_reason) {
  DVLOG(1) << __func__ << ": " << disconnect_reason;
  if (device_change_cb_)
    std::move(device_change_cb_).Run();
  return S_OK;
}

STDMETHODIMP AudioSessionEventListener::OnSimpleVolumeChanged(
    float new_volume,
    BOOL new_mute,
    LPCGUID event_context) {
  return S_OK;
}

STDMETHODIMP AudioSessionEventListener::OnStateChanged(
    AudioSessionState new_state) {
  return S_OK;
}

}  // namespace media
