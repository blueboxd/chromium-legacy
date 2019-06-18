// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <memory>

#include "base/test/scoped_task_environment.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/gpu/chromeos/platform_video_frame_pool.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class PlatformVideoFramePoolTest
    : public ::testing::TestWithParam<VideoPixelFormat> {
 public:
  PlatformVideoFramePoolTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME) {
    // Seed test clock with some dummy non-zero value to avoid confusion with
    // empty base::TimeTicks values.
    test_clock_.Advance(base::TimeDelta::FromSeconds(1234));

    pool_.reset(new PlatformVideoFramePool(
        base::BindRepeating(&VideoFrame::CreateZeroInitializedFrame),
        &test_clock_));
    pool_->set_parent_task_runner(base::ThreadTaskRunnerHandle::Get());
  }

  void SetFrameFormat(VideoPixelFormat format) {
    gfx::Size coded_size(320, 240);
    visible_rect_.set_size(coded_size);
    natural_size_ = coded_size;
    layout_ = VideoFrameLayout::Create(format, coded_size);
    DCHECK(layout_);

    pool_->SetFrameFormat(layout_.value(), visible_rect_, natural_size_);
  }

  scoped_refptr<VideoFrame> GetFrame(int timestamp_ms) {
    scoped_refptr<VideoFrame> frame = pool_->GetFrame();
    frame->set_timestamp(base::TimeDelta::FromMilliseconds(timestamp_ms));

    EXPECT_EQ(layout_.value().format(), frame->format());
    EXPECT_EQ(layout_.value().coded_size(), frame->coded_size());
    EXPECT_EQ(visible_rect_, frame->visible_rect());
    EXPECT_EQ(natural_size_, frame->natural_size());

    return frame;
  }

  void CheckPoolSize(size_t size) const {
    EXPECT_EQ(size, pool_->GetPoolSizeForTesting());
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  base::SimpleTestTickClock test_clock_;
  std::unique_ptr<PlatformVideoFramePool,
                  std::default_delete<DmabufVideoFramePool>>
      pool_;

  base::Optional<VideoFrameLayout> layout_;
  gfx::Rect visible_rect_;
  gfx::Size natural_size_;
};

TEST_P(PlatformVideoFramePoolTest, FrameInitializedAndZeroed) {
  SetFrameFormat(GetParam());
  scoped_refptr<VideoFrame> frame = GetFrame(10);

  // Verify that frame is initialized with zeros.
  for (size_t i = 0; i < VideoFrame::NumPlanes(frame->format()); ++i)
    EXPECT_EQ(0, frame->data(i)[0]);
}

INSTANTIATE_TEST_SUITE_P(,
                         PlatformVideoFramePoolTest,
                         testing::Values(PIXEL_FORMAT_I420,
                                         PIXEL_FORMAT_NV12,
                                         PIXEL_FORMAT_ARGB));

TEST_F(PlatformVideoFramePoolTest, SingleFrameReuse) {
  SetFrameFormat(PIXEL_FORMAT_I420);
  scoped_refptr<VideoFrame> frame = GetFrame(10);
  const uint8_t* old_y_data = frame->data(VideoFrame::kYPlane);

  // Clear frame reference to return the frame to the pool.
  frame = nullptr;
  scoped_task_environment_.RunUntilIdle();

  // Verify that the next frame from the pool uses the same memory.
  scoped_refptr<VideoFrame> new_frame = GetFrame(20);
  EXPECT_EQ(old_y_data, new_frame->data(VideoFrame::kYPlane));
}

TEST_F(PlatformVideoFramePoolTest, MultipleFrameReuse) {
  SetFrameFormat(PIXEL_FORMAT_I420);
  scoped_refptr<VideoFrame> frame1 = GetFrame(10);
  scoped_refptr<VideoFrame> frame2 = GetFrame(20);
  const uint8_t* old_y_data1 = frame1->data(VideoFrame::kYPlane);
  const uint8_t* old_y_data2 = frame2->data(VideoFrame::kYPlane);

  frame1 = nullptr;
  scoped_task_environment_.RunUntilIdle();
  frame1 = GetFrame(30);
  EXPECT_EQ(old_y_data1, frame1->data(VideoFrame::kYPlane));

  frame2 = nullptr;
  scoped_task_environment_.RunUntilIdle();
  frame2 = GetFrame(40);
  EXPECT_EQ(old_y_data2, frame2->data(VideoFrame::kYPlane));

  frame1 = nullptr;
  frame2 = nullptr;
  scoped_task_environment_.RunUntilIdle();
  CheckPoolSize(2u);
}

TEST_F(PlatformVideoFramePoolTest, FormatChange) {
  SetFrameFormat(PIXEL_FORMAT_I420);
  scoped_refptr<VideoFrame> frame_a = GetFrame(10);
  scoped_refptr<VideoFrame> frame_b = GetFrame(10);

  // Clear frame references to return the frames to the pool.
  frame_a = nullptr;
  frame_b = nullptr;
  scoped_task_environment_.RunUntilIdle();

  // Verify that both frames are in the pool.
  CheckPoolSize(2u);

  // Verify that requesting a frame with a different format causes the pool
  // to get drained.
  SetFrameFormat(PIXEL_FORMAT_I420A);
  scoped_refptr<VideoFrame> new_frame = GetFrame(10);
  CheckPoolSize(0u);
}

TEST_F(PlatformVideoFramePoolTest, FrameValidAfterPoolDestruction) {
  SetFrameFormat(PIXEL_FORMAT_I420);
  scoped_refptr<VideoFrame> frame = GetFrame(10);

  // Destroy the pool.
  pool_.reset();

  // Write to the Y plane. The memory tools should detect a
  // use-after-free if the storage was actually removed by pool destruction.
  memset(frame->data(VideoFrame::kYPlane), 0xff,
         frame->rows(VideoFrame::kYPlane) * frame->stride(VideoFrame::kYPlane));
}

TEST_F(PlatformVideoFramePoolTest, StaleFramesAreExpired) {
  SetFrameFormat(PIXEL_FORMAT_I420);
  scoped_refptr<VideoFrame> frame_1 = GetFrame(10);
  scoped_refptr<VideoFrame> frame_2 = GetFrame(10);
  EXPECT_NE(frame_1.get(), frame_2.get());
  CheckPoolSize(0u);

  // Drop frame and verify that resources are still available for reuse.
  frame_1 = nullptr;
  scoped_task_environment_.RunUntilIdle();
  CheckPoolSize(1u);

  // Advance clock far enough to hit stale timer; ensure only frame_1 has its
  // resources released.
  base::TimeDelta time_forward = base::TimeDelta::FromMinutes(1);
  test_clock_.Advance(time_forward);
  scoped_task_environment_.FastForwardBy(time_forward);
  frame_2 = nullptr;
  scoped_task_environment_.RunUntilIdle();
  CheckPoolSize(1u);
}

}  // namespace media
