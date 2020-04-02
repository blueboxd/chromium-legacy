// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/rtc_encoded_audio_stream_transformer.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_scoped_refptr_cross_thread_copier.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"

namespace blink {

namespace {

// This delegate class exists to work around the fact that
// RTCEncodedAudioStreamTransformer cannot derive from rtc::RefCountedObject
// and post tasks referencing itself as an rtc::scoped_refptr. Instead,
// RTCEncodedAudioStreamTransformer creates a delegate using
// rtc::RefCountedObject and posts tasks referencing the delegate, which invokes
// the RTCEncodedAudioStreamTransformer via callbacks.
class RTCEncodedAudioStreamTransformerDelegate
    : public webrtc::FrameTransformerInterface {
 public:
  RTCEncodedAudioStreamTransformerDelegate(
      const base::WeakPtr<RTCEncodedAudioStreamTransformer>& transformer,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner)
      : transformer_(transformer),
        main_task_runner_(std::move(main_task_runner)) {
    DCHECK(main_task_runner_->BelongsToCurrentThread());
  }

  // webrtc::FrameTransformerInterface
  void RegisterTransformedFrameCallback(
      rtc::scoped_refptr<webrtc::TransformedFrameCallback>
          send_frame_to_sink_callback) override {
    PostCrossThreadTask(
        *main_task_runner_, FROM_HERE,
        CrossThreadBindOnce(
            &RTCEncodedAudioStreamTransformer::RegisterTransformedFrameCallback,
            transformer_, std::move(send_frame_to_sink_callback)));
  }

  void UnregisterTransformedFrameCallback() override {
    PostCrossThreadTask(
        *main_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&RTCEncodedAudioStreamTransformer::
                                UnregisterTransformedFrameCallback,
                            transformer_));
  }

  void Transform(
      std::unique_ptr<webrtc::TransformableFrameInterface> frame) override {
    auto audio_frame = base::WrapUnique(
        static_cast<webrtc::TransformableFrameInterface*>(frame.release()));
    PostCrossThreadTask(
        *main_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&RTCEncodedAudioStreamTransformer::TransformFrame,
                            transformer_, std::move(audio_frame)));
  }

 private:
  base::WeakPtr<RTCEncodedAudioStreamTransformer> transformer_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
};

}  // namespace

RTCEncodedAudioStreamTransformer::RTCEncodedAudioStreamTransformer(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner) {
  DCHECK(main_task_runner->BelongsToCurrentThread());
  delegate_ =
      new rtc::RefCountedObject<RTCEncodedAudioStreamTransformerDelegate>(
          weak_factory_.GetWeakPtr(), std::move(main_task_runner));
}

void RTCEncodedAudioStreamTransformer::RegisterTransformedFrameCallback(
    rtc::scoped_refptr<webrtc::TransformedFrameCallback> callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  send_frame_to_sink_cb_ = callback;
}

void RTCEncodedAudioStreamTransformer::UnregisterTransformedFrameCallback() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  send_frame_to_sink_cb_ = nullptr;
}

void RTCEncodedAudioStreamTransformer::TransformFrame(
    std::unique_ptr<webrtc::TransformableFrameInterface> frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // If no transformer callback has been set, drop the frame.
  if (!transformer_callback_)
    return;

  transformer_callback_.Run(std::move(frame));
}

void RTCEncodedAudioStreamTransformer::SendFrameToSink(
    std::unique_ptr<webrtc::TransformableFrameInterface> frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (send_frame_to_sink_cb_)
    send_frame_to_sink_cb_->OnTransformedFrame(std::move(frame));
}

void RTCEncodedAudioStreamTransformer::SetTransformerCallback(
    TransformerCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  transformer_callback_ = std::move(callback);
}

void RTCEncodedAudioStreamTransformer::ResetTransformerCallback() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  transformer_callback_.Reset();
}

bool RTCEncodedAudioStreamTransformer::HasTransformerCallback() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return !transformer_callback_.is_null();
}

bool RTCEncodedAudioStreamTransformer::HasTransformedFrameCallback() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return !!send_frame_to_sink_cb_;
}

rtc::scoped_refptr<webrtc::FrameTransformerInterface>
RTCEncodedAudioStreamTransformer::Delegate() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return delegate_;
}

}  // namespace blink
