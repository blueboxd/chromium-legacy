// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_WIN_AUDIO_SESSION_EVENT_LISTENER_WIN_H_
#define MEDIA_AUDIO_WIN_AUDIO_SESSION_EVENT_LISTENER_WIN_H_

#include <audiopolicy.h>
#include <wrl/client.h>

#include "base/callback.h"
#include "media/base/media_export.h"

namespace media {

class MEDIA_EXPORT AudioSessionEventListener : public IAudioSessionEvents {
 public:
  // Calls RegisterAudioSessionNotification() on |client| and calls
  // |device_change_cb| when OnSessionDisconnected() is called.
  //
  // Since the IAudioClient session is dead after the disconnection, we use a
  // OnceCallback. The delivery of this notification is fatal to the |client|.
  AudioSessionEventListener(IAudioClient* client,
                            base::OnceClosure device_change_cb);
  virtual ~AudioSessionEventListener();

 private:
  friend class AudioSessionEventListenerTest;

  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;
  STDMETHODIMP QueryInterface(REFIID iid, void** object) override;

  // IAudioSessionEvents implementation.
  STDMETHODIMP OnChannelVolumeChanged(DWORD channel_count,
                                      float new_channel_volume_array[],
                                      DWORD changed_channel,
                                      LPCGUID event_context) override;
  STDMETHODIMP OnDisplayNameChanged(LPCWSTR new_display_name,
                                    LPCGUID event_context) override;
  STDMETHODIMP OnGroupingParamChanged(LPCGUID new_grouping_param,
                                      LPCGUID event_context) override;
  STDMETHODIMP OnIconPathChanged(LPCWSTR new_icon_path,
                                 LPCGUID event_context) override;
  STDMETHODIMP OnSessionDisconnected(
      AudioSessionDisconnectReason disconnect_reason) override;
  STDMETHODIMP OnSimpleVolumeChanged(float new_volume,
                                     BOOL new_mute,
                                     LPCGUID event_context) override;
  STDMETHODIMP OnStateChanged(AudioSessionState new_state) override;

  base::OnceClosure device_change_cb_;
  Microsoft::WRL::ComPtr<IAudioSessionControl> audio_session_control_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_WIN_AUDIO_SESSION_EVENT_LISTENER_WIN_H_
