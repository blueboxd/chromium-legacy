// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_CONTROL_CONNECTION_H_
#define CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_CONTROL_CONNECTION_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "chromecast/media/audio/mixer_service/mixer_connection.h"
#include "chromecast/media/audio/mixer_service/mixer_socket.h"
#include "chromecast/public/volume_control.h"

namespace chromecast {
namespace media {
namespace mixer_service {

// Mixer service connection for controlling general mixer properties, such as
// device volume and postprocessor configuration. Not thread-safe; all usage of
// a given instance must be on the same sequence. Must be created on an IO
// thread.
class ControlConnection : public MixerConnection, public MixerSocket::Delegate {
 public:
  using ConnectedCallback = base::RepeatingClosure;

  // Callback to receive mixer stream count changes.
  using StreamCountCallback =
      base::RepeatingCallback<void(int primary_streams, int sfx_streams)>;

  ControlConnection();
  ~ControlConnection() override;

  // Connects to the mixer. If the mixer connection is lost, this will
  // automatically reconnect. If |callback| is nonempty, it will be called each
  // time a connection is (re)established with the mixer. This can be used to
  // re-send preprocessor configuration, since it is not persisted across
  // disconnects.
  void Connect(ConnectedCallback callback = ConnectedCallback());

  // Sets volume multiplier for all streams of a given content type.
  void SetVolume(AudioContentType type, float volume_multiplier);

  // Sets mute state all streams of a given content type.
  void SetMuted(AudioContentType type, bool muted);

  // Sets the maximum effective volume multiplier for a given content type.
  void SetVolumeLimit(AudioContentType type, float max_volume_multiplier);

  // Sends arbitrary config data to a specific postprocessor. Note that the
  // config is not persisted across disconnects, and is not saved if
  // ConfigurePostprocessor() is called when not connected to the mixer, so
  // use the Connect() callback to determine when to (re)send config, if needed.
  void ConfigurePostprocessor(const std::string& name,
                              const void* config,
                              int size_bytes);

  // Instructs the mixer to reload postprocessors based on the config file.
  void ReloadPostprocessors();

  // Sets a callback to receive mixer stream count changes. |callback| may be an
  // empty callback to remove it.
  void SetStreamCountCallback(StreamCountCallback callback);

 private:
  // MixerConnection implementation:
  void OnConnected(std::unique_ptr<MixerSocket> socket) override;
  void OnConnectionError() override;

  // MixerSocket::Delegate implementation:
  bool HandleMetadata(const Generic& message) override;

  std::unique_ptr<MixerSocket> socket_;

  ConnectedCallback connect_callback_;

  base::flat_map<AudioContentType, float> volume_;
  base::flat_map<AudioContentType, bool> muted_;
  base::flat_map<AudioContentType, float> volume_limit_;

  StreamCountCallback stream_count_callback_;

  DISALLOW_COPY_AND_ASSIGN(ControlConnection);
};

}  // namespace mixer_service
}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_CONTROL_CONNECTION_H_
