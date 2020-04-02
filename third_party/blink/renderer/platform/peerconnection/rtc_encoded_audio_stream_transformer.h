// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_ENCODED_AUDIO_STREAM_TRANSFORMER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_ENCODED_AUDIO_STREAM_TRANSFORMER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/webrtc/api/scoped_refptr.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace webrtc {
class FrameTransformerInterface;
class TransformedFrameCallback;
class TransformableFrameInterface;
}  // namespace webrtc

namespace blink {

class PLATFORM_EXPORT RTCEncodedAudioStreamTransformer {
 public:
  using TransformerCallback = base::RepeatingCallback<void(
      std::unique_ptr<webrtc::TransformableFrameInterface>)>;
  explicit RTCEncodedAudioStreamTransformer(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner);

  // Called by WebRTC to let us know about a callback object to send transformed
  // frames to the WebRTC decoder. Runs on an internal WebRTC thread.
  // The callback can run on any thread.
  void RegisterTransformedFrameCallback(
      rtc::scoped_refptr<webrtc::TransformedFrameCallback>);

  // Called by WebRTC to let us know that any reference to the callback object
  // reported by RegisterTransformedFrameCallback() should be released since
  // the callback is no longer useful and is intended for destruction.
  void UnregisterTransformedFrameCallback();

  // Called by WebRTC to notify of new untransformed frames from the WebRTC
  // stack. Runs on an internal WebRTC thread.
  void TransformFrame(std::unique_ptr<webrtc::TransformableFrameInterface>);

  // Send a transformed frame to the WebRTC sink. Must run on the main
  // thread.
  void SendFrameToSink(
      std::unique_ptr<webrtc::TransformableFrameInterface> frame);

  // Set a callback to be invoked on every untransformed frame. Must run on the
  // main thread.
  void SetTransformerCallback(TransformerCallback);

  // Removes the callback
  void ResetTransformerCallback();

  // Returns true if a callback has been set with SetTransformerCallback(),
  // false otherwise. Must run on the main thread.
  bool HasTransformerCallback() const;

  // Returns true if a webrtc::TransformedFrameCallback is registered.
  bool HasTransformedFrameCallback() const;

  rtc::scoped_refptr<webrtc::FrameTransformerInterface> Delegate();

 private:
  THREAD_CHECKER(thread_checker_);
  rtc::scoped_refptr<webrtc::FrameTransformerInterface> delegate_;
  rtc::scoped_refptr<webrtc::TransformedFrameCallback> send_frame_to_sink_cb_;
  TransformerCallback transformer_callback_;
  base::WeakPtrFactory<RTCEncodedAudioStreamTransformer> weak_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_ENCODED_AUDIO_STREAM_TRANSFORMER_H_
