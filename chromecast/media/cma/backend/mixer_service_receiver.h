// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MIXER_SERVICE_RECEIVER_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MIXER_SERVICE_RECEIVER_H_

#include <memory>

#include "base/macros.h"
#include "chromecast/media/audio/mixer_service/receiver/receiver.h"

namespace chromecast {
namespace media {
class StreamMixer;

namespace mixer_service {
class Generic;
class MixerSocket;
}  // namespace mixer_service

class MixerServiceReceiver : public mixer_service::Receiver {
 public:
  explicit MixerServiceReceiver(StreamMixer* mixer);
  ~MixerServiceReceiver() override;

 private:
  // mixer_service::Receiver implementation:
  void CreateOutputStream(std::unique_ptr<mixer_service::MixerSocket> socket,
                          const mixer_service::Generic& message) override;
  void CreateLoopbackConnection(
      std::unique_ptr<mixer_service::MixerSocket> socket,
      const mixer_service::Generic& message) override;
  void CreateAudioRedirection(
      std::unique_ptr<mixer_service::MixerSocket> socket,
      const mixer_service::Generic& message) override;
  void CreateControlConnection(
      std::unique_ptr<mixer_service::MixerSocket> socket,
      const mixer_service::Generic& message) override;

  StreamMixer* const mixer_;

  DISALLOW_COPY_AND_ASSIGN(MixerServiceReceiver);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MIXER_SERVICE_RECEIVER_H_
