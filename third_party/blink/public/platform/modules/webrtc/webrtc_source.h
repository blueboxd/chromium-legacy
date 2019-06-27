// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_WEBRTC_WEBRTC_SOURCE_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_WEBRTC_WEBRTC_SOURCE_H_

#include "third_party/blink/public/platform/web_common.h"

namespace media {
class AudioBus;
}

namespace blink {

// TODO(xians): Move the following interface to webrtc so that
// libjingle can own references to the renderer and capturer.
//
// TODO(xians): Merge this interface with WebRtcAudioRendererSource.
// The reason why we could not do it today is that WebRtcAudioRendererSource
// gets the data by pulling, while the data is pushed into
// WebRtcPlayoutDataSource::Sink.
//
// TODO(crbug.com/704136): Move the class below out of the Blink exposed
// API when all users of it have been Onion souped.
class BLINK_PLATFORM_EXPORT WebRtcPlayoutDataSource {
 public:
  class Sink {
   public:
    // Callback to get the playout data.
    // Called on the audio render thread.
    // |audio_bus| must have buffer size |sample_rate/100| and 1-2 channels.
    virtual void OnPlayoutData(media::AudioBus* audio_bus,
                               int sample_rate,
                               int audio_delay_milliseconds) = 0;

    // Callback to notify the sink that the source has changed.
    // Called on the main render thread.
    virtual void OnPlayoutDataSourceChanged() = 0;

    // Called to notify that the audio render thread has changed, and
    // OnPlayoutData() will from now on be called on the new thread.
    // Called on the new audio render thread.
    virtual void OnRenderThreadChanged() = 0;

   protected:
    virtual ~Sink() {}
  };

  // Adds/Removes the sink of WebRtcAudioRendererSource to the ADM.
  // These methods are used by the MediaStreamAudioProcesssor to get the
  // rendered data for AEC.
  virtual void AddPlayoutSink(Sink* sink) = 0;
  virtual void RemovePlayoutSink(Sink* sink) = 0;

 protected:
  virtual ~WebRtcPlayoutDataSource() {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_WEBRTC_WEBRTC_SOURCE_H_
