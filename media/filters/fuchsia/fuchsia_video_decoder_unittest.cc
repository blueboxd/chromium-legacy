// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/fuchsia/fuchsia_video_decoder.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/test/scoped_task_environment.h"
#include "media/base/test_data_util.h"
#include "media/base/test_helpers.h"
#include "media/base/video_decoder.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class FuchsiaVideoDecoderTest : public testing::Test {
 public:
  FuchsiaVideoDecoderTest() {
    decoder_ = CreateFuchsiaVideoDecoderForTests(/*enable_sw_decoding=*/true);
  }
  ~FuchsiaVideoDecoderTest() override = default;

  bool Initialize(VideoDecoderConfig config) WARN_UNUSED_RESULT {
    base::RunLoop run_loop;
    bool init_cb_result = false;
    decoder_->Initialize(
        config, true, /*cdm_context=*/nullptr,
        base::BindRepeating(
            [](bool* init_cb_result, base::RunLoop* run_loop, bool result) {
              *init_cb_result = result;
              run_loop->Quit();
            },
            &init_cb_result, &run_loop),
        base::BindRepeating(&FuchsiaVideoDecoderTest::OnVideoFrame,
                            base::Unretained(this)),
        base::NullCallback());

    run_loop.Run();
    return init_cb_result;
  }

  void OnVideoFrame(scoped_refptr<VideoFrame> frame) {
    num_output_frames_++;
    output_frames_.push_back(std::move(frame));
    while (output_frames_.size() > frames_to_keep_) {
      output_frames_.pop_front();
    }
    if (on_frame_)
      on_frame_.Run();
  }

  DecodeStatus DecodeBuffer(scoped_refptr<DecoderBuffer> buffer) {
    base::RunLoop run_loop;
    DecodeStatus status;
    decoder_->Decode(buffer,
                     base::BindRepeating(
                         [](DecodeStatus* status, base::RunLoop* run_loop,
                            DecodeStatus result) {
                           *status = result;
                           run_loop->Quit();
                         },
                         &status, &run_loop));

    run_loop.Run();

    return status;
  }

  DecodeStatus ReadAndDecodeFrame(const std::string& name) {
    return DecodeBuffer(ReadTestDataFile(name));
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::ThreadingMode::MAIN_THREAD_ONLY,
      base::test::TaskEnvironment::MainThreadType::IO};
  std::unique_ptr<VideoDecoder> decoder_;

  std::list<scoped_refptr<VideoFrame>> output_frames_;
  int num_output_frames_ = 0;

  // Number of frames that OnVideoFrame() should keep in |output_frames_|.
  size_t frames_to_keep_ = 2;

  base::RepeatingClosure on_frame_;
};

TEST_F(FuchsiaVideoDecoderTest, CreateAndDestroy) {}

TEST_F(FuchsiaVideoDecoderTest, CreateInitDestroy) {
  EXPECT_TRUE(Initialize(TestVideoConfig::NormalH264()));
}

TEST_F(FuchsiaVideoDecoderTest, DISABLED_VP9) {
  ASSERT_TRUE(Initialize(TestVideoConfig::Normal(kCodecVP9)));

  ASSERT_TRUE(ReadAndDecodeFrame("vp9-I-frame-320x240") == DecodeStatus::OK);
  ASSERT_TRUE(DecodeBuffer(DecoderBuffer::CreateEOSBuffer()) ==
              DecodeStatus::OK);

  EXPECT_EQ(num_output_frames_, 1);
}

TEST_F(FuchsiaVideoDecoderTest, H264) {
  ASSERT_TRUE(Initialize(TestVideoConfig::NormalH264()));

  ASSERT_TRUE(ReadAndDecodeFrame("h264-320x180-frame-0") == DecodeStatus::OK);
  ASSERT_TRUE(ReadAndDecodeFrame("h264-320x180-frame-1") == DecodeStatus::OK);
  ASSERT_TRUE(ReadAndDecodeFrame("h264-320x180-frame-2") == DecodeStatus::OK);
  ASSERT_TRUE(ReadAndDecodeFrame("h264-320x180-frame-3") == DecodeStatus::OK);
  ASSERT_TRUE(DecodeBuffer(DecoderBuffer::CreateEOSBuffer()) ==
              DecodeStatus::OK);

  EXPECT_EQ(num_output_frames_, 4);
}

}  // namespace media
