// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/rtc_encoded_video_stream_transformer.h"

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
// RTCEncodedVideoStreamTransformer cannot derive from rtc::RefCountedObject
// and post tasks referencing itself as an rtc::scoped_refptr. Instead,
// RTCEncodedVideoStreamTransformer creates a delegate using
// rtc::RefCountedObject and posts tasks referencing the delegate, which invokes
// the RTCEncodedVideoStreamTransformer via callbacks.
class RTCEncodedVideoStreamTransformerDelegate
    : public webrtc::FrameTransformerInterface {
 public:
  RTCEncodedVideoStreamTransformerDelegate(
      const base::WeakPtr<RTCEncodedVideoStreamTransformer>& transformer,
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
            &RTCEncodedVideoStreamTransformer::RegisterTransformedFrameCallback,
            transformer_, std::move(send_frame_to_sink_callback)));
  }

  void UnregisterTransformedFrameCallback() override {
    PostCrossThreadTask(
        *main_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&RTCEncodedVideoStreamTransformer::
                                UnregisterTransformedFrameCallback,
                            transformer_));
  }

  void Transform(
      std::unique_ptr<webrtc::TransformableFrameInterface> frame) override {
    auto video_frame =
        base::WrapUnique(static_cast<webrtc::TransformableVideoFrameInterface*>(
            frame.release()));
    PostCrossThreadTask(
        *main_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&RTCEncodedVideoStreamTransformer::TransformFrame,
                            transformer_, std::move(video_frame)));
  }

 private:
  base::WeakPtr<RTCEncodedVideoStreamTransformer> transformer_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
};

}  // namespace

RTCEncodedVideoStreamTransformer::RTCEncodedVideoStreamTransformer(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner) {
  DCHECK(main_task_runner->BelongsToCurrentThread());
  delegate_ =
      new rtc::RefCountedObject<RTCEncodedVideoStreamTransformerDelegate>(
          weak_factory_.GetWeakPtr(), std::move(main_task_runner));
}

void RTCEncodedVideoStreamTransformer::RegisterTransformedFrameCallback(
    rtc::scoped_refptr<webrtc::TransformedFrameCallback> callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  send_frame_to_sink_cb_ = callback;
}

void RTCEncodedVideoStreamTransformer::UnregisterTransformedFrameCallback() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  send_frame_to_sink_cb_ = nullptr;
}

void RTCEncodedVideoStreamTransformer::TransformFrame(
    std::unique_ptr<webrtc::TransformableVideoFrameInterface> frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // If no transformer callback has been set, drop the frame.
  if (!transformer_callback_)
    return;

  transformer_callback_.Run(std::move(frame));
}

void RTCEncodedVideoStreamTransformer::SendFrameToSink(
    std::unique_ptr<webrtc::TransformableVideoFrameInterface> frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (send_frame_to_sink_cb_)
    send_frame_to_sink_cb_->OnTransformedFrame(std::move(frame));
}

void RTCEncodedVideoStreamTransformer::SetTransformerCallback(
    TransformerCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  transformer_callback_ = std::move(callback);
}

void RTCEncodedVideoStreamTransformer::ResetTransformerCallback() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  transformer_callback_.Reset();
}

bool RTCEncodedVideoStreamTransformer::HasTransformerCallback() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return !transformer_callback_.is_null();
}

bool RTCEncodedVideoStreamTransformer::HasTransformedFrameCallback() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return !!send_frame_to_sink_cb_;
}

rtc::scoped_refptr<webrtc::FrameTransformerInterface>
RTCEncodedVideoStreamTransformer::Delegate() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return delegate_;
}

}  // namespace blink
