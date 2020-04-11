// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/rtc_encoded_video_stream_transformer.h"

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_scoped_refptr_cross_thread_copier.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"

namespace blink {

namespace {

const uint32_t kSSRC = 1;
const uint32_t kNonexistentSSRC = 0;

class MockWebRtcTransformedFrameCallback
    : public webrtc::TransformedFrameCallback {
 public:
  MOCK_METHOD1(OnTransformedFrame,
               void(std::unique_ptr<webrtc::TransformableFrameInterface>));
};

class MockTransformerCallbackHolder {
 public:
  MOCK_METHOD1(OnEncodedFrame,
               void(std::unique_ptr<webrtc::TransformableVideoFrameInterface>));
};

class FakeVideoFrame : public webrtc::TransformableVideoFrameInterface {
 public:
  explicit FakeVideoFrame(uint32_t ssrc) : ssrc_(ssrc) {}

  rtc::ArrayView<const uint8_t> GetData() const override {
    return rtc::ArrayView<const uint8_t>();
  }

  // Copies |data| into the owned frame payload data.
  void SetData(rtc::ArrayView<const uint8_t> data) override {}
  uint32_t GetTimestamp() const override { return 0; }
  uint32_t GetSsrc() const override { return ssrc_; }
  bool IsKeyFrame() const override { return true; }
  std::vector<uint8_t> GetAdditionalData() const override {
    return std::vector<uint8_t>();
  }

 private:
  uint32_t ssrc_;
};

std::unique_ptr<webrtc::TransformableVideoFrameInterface> CreateFakeFrame() {
  return std::make_unique<FakeVideoFrame>(kSSRC);
}

}  // namespace

class RTCEncodedVideoStreamTransformerTest : public ::testing::Test {
 public:
  RTCEncodedVideoStreamTransformerTest()
      : main_task_runner_(
            blink::scheduler::GetSingleThreadTaskRunnerForTesting()),
        webrtc_task_runner_(base::ThreadPool::CreateSingleThreadTaskRunner({})),
        webrtc_callback_(
            new rtc::RefCountedObject<MockWebRtcTransformedFrameCallback>()),
        encoded_video_stream_transformer_(main_task_runner_) {}

  void SetUp() override {
    EXPECT_FALSE(
        encoded_video_stream_transformer_.HasTransformedFrameSinkCallback(
            kSSRC));
    encoded_video_stream_transformer_.RegisterTransformedFrameSinkCallback(
        webrtc_callback_, kSSRC);
    EXPECT_TRUE(
        encoded_video_stream_transformer_.HasTransformedFrameSinkCallback(
            kSSRC));
    EXPECT_FALSE(
        encoded_video_stream_transformer_.HasTransformedFrameSinkCallback(
            kNonexistentSSRC));
  }

  void TearDown() override {
    encoded_video_stream_transformer_.UnregisterTransformedFrameSinkCallback(
        kSSRC);
    EXPECT_FALSE(
        encoded_video_stream_transformer_.HasTransformedFrameSinkCallback(
            kSSRC));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> webrtc_task_runner_;
  rtc::scoped_refptr<MockWebRtcTransformedFrameCallback> webrtc_callback_;
  MockTransformerCallbackHolder mock_transformer_callback_holder_;
  RTCEncodedVideoStreamTransformer encoded_video_stream_transformer_;
};

TEST_F(RTCEncodedVideoStreamTransformerTest,
       TransformerForwardsFrameToTransformerCallback) {
  EXPECT_FALSE(encoded_video_stream_transformer_.HasTransformerCallback());
  encoded_video_stream_transformer_.SetTransformerCallback(
      WTF::BindRepeating(&MockTransformerCallbackHolder::OnEncodedFrame,
                         WTF::Unretained(&mock_transformer_callback_holder_)));
  EXPECT_TRUE(encoded_video_stream_transformer_.HasTransformerCallback());

  EXPECT_CALL(mock_transformer_callback_holder_, OnEncodedFrame);
  // Frames are pushed to the RTCEncodedVideoStreamTransformer via its delegate,
  // which  would normally be registered with a WebRTC sender or receiver.
  // In this test, manually send the frame to the transformer on the simulated
  // WebRTC thread.
  PostCrossThreadTask(
      *webrtc_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&webrtc::FrameTransformerInterface::Transform,
                          encoded_video_stream_transformer_.Delegate(),
                          CreateFakeFrame()));
  task_environment_.RunUntilIdle();
}

TEST_F(RTCEncodedVideoStreamTransformerTest, TransformerForwardsFrameToWebRTC) {
  EXPECT_CALL(*webrtc_callback_, OnTransformedFrame);
  encoded_video_stream_transformer_.SendFrameToSink(CreateFakeFrame());
  task_environment_.RunUntilIdle();
}

}  // namespace blink
