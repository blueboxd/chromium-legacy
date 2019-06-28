// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_EXTRA_AUDIO_STREAM_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_EXTRA_AUDIO_STREAM_H_

namespace chromecast {
namespace media {

class ExtraAudioStream {
 public:
  virtual void SetGlobalVolumeMultiplier(float multiplier) = 0;

 protected:
  virtual ~ExtraAudioStream() = default;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_EXTRA_AUDIO_STREAM_H_
