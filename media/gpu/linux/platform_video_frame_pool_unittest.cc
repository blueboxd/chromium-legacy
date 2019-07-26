// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/gpu/linux/platform_video_frame_pool.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

base::ScopedFD CreateTmpHandle() {
  base::FilePath path;
  DCHECK(CreateTemporaryFile(&path));
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  DCHECK(file.IsValid());
  return base::ScopedFD(file.TakePlatformFile());
}

scoped_refptr<VideoFrame> CreateDmabufVideoFrame(VideoPixelFormat format,
                                                 const gfx::Size& coded_size,
                                                 const gfx::Rect& visible_rect,
                                                 const gfx::Size& natural_size,
                                                 base::TimeDelta timestamp) {
  base::Optional<VideoFrameLayout> layout =
      VideoFrameLayout::Create(format, coded_size);
  DCHECK(layout);

  std::vector<base::ScopedFD> dmabuf_fds;
  for (size_t i = 0; i < VideoFrame::NumPlanes(format); ++i)
    dmabuf_fds.push_back(CreateTmpHandle());

  return VideoFrame::WrapExternalDmabufs(*layout, visible_rect, natural_size,
                                         std::move(dmabuf_fds), timestamp);
}

}  // namespace

class PlatformVideoFramePoolTest
    : public ::testing::TestWithParam<VideoPixelFormat> {
 public:
  using DmabufId = PlatformVideoFramePool::DmabufId;

  PlatformVideoFramePoolTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::TimeSource::MOCK_TIME) {
    pool_.reset(new PlatformVideoFramePool(
        base::BindRepeating(&CreateDmabufVideoFrame), &test_clock_));
    pool_->set_parent_task_runner(base::ThreadTaskRunnerHandle::Get());
  }

  void SetFrameFormat(VideoPixelFormat format) {
    gfx::Size coded_size(320, 240);
    visible_rect_.set_size(coded_size);
    natural_size_ = coded_size;
    layout_ = VideoFrameLayout::Create(format, coded_size);
    DCHECK(layout_);

    pool_->NegotiateFrameFormat(*layout_, visible_rect_, natural_size_);
  }

  scoped_refptr<VideoFrame> GetFrame(int timestamp_ms) {
    scoped_refptr<VideoFrame> frame = pool_->GetFrame();
    frame->set_timestamp(base::TimeDelta::FromMilliseconds(timestamp_ms));

    EXPECT_EQ(layout_->format(), frame->format());
    EXPECT_EQ(layout_->coded_size(), frame->coded_size());
    EXPECT_EQ(visible_rect_, frame->visible_rect());
    EXPECT_EQ(natural_size_, frame->natural_size());

    return frame;
  }

  void CheckPoolSize(size_t size) const {
    EXPECT_EQ(size, pool_->GetPoolSizeForTesting());
  }

  DmabufId GetDmabufId(const VideoFrame& frame) { return &(frame.DmabufFds()); }

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

INSTANTIATE_TEST_SUITE_P(,
                         PlatformVideoFramePoolTest,
                         testing::Values(PIXEL_FORMAT_I420,
                                         PIXEL_FORMAT_NV12,
                                         PIXEL_FORMAT_ARGB));

TEST_F(PlatformVideoFramePoolTest, SingleFrameReuse) {
  SetFrameFormat(PIXEL_FORMAT_I420);
  scoped_refptr<VideoFrame> frame = GetFrame(10);
  DmabufId id = GetDmabufId(*frame);

  // Clear frame reference to return the frame to the pool.
  frame = nullptr;
  scoped_task_environment_.RunUntilIdle();

  // Verify that the next frame from the pool uses the same memory.
  scoped_refptr<VideoFrame> new_frame = GetFrame(20);
  EXPECT_EQ(id, GetDmabufId(*new_frame));
}

TEST_F(PlatformVideoFramePoolTest, MultipleFrameReuse) {
  SetFrameFormat(PIXEL_FORMAT_I420);
  scoped_refptr<VideoFrame> frame1 = GetFrame(10);
  scoped_refptr<VideoFrame> frame2 = GetFrame(20);
  DmabufId id1 = GetDmabufId(*frame1);
  DmabufId id2 = GetDmabufId(*frame2);

  frame1 = nullptr;
  scoped_task_environment_.RunUntilIdle();
  frame1 = GetFrame(30);
  EXPECT_EQ(id1, GetDmabufId(*frame1));

  frame2 = nullptr;
  scoped_task_environment_.RunUntilIdle();
  frame2 = GetFrame(40);
  EXPECT_EQ(id2, GetDmabufId(*frame2));

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

TEST_F(PlatformVideoFramePoolTest, UnwrapVideoFrame) {
  SetFrameFormat(PIXEL_FORMAT_I420);
  scoped_refptr<VideoFrame> frame_1 = GetFrame(10);
  scoped_refptr<VideoFrame> frame_2 = VideoFrame::WrapVideoFrame(
      *frame_1, frame_1->format(), frame_1->visible_rect(),
      frame_1->natural_size());
  EXPECT_EQ(pool_->UnwrapFrame(*frame_1), pool_->UnwrapFrame(*frame_2));
  EXPECT_TRUE(frame_1->IsSameDmaBufsAs(*frame_2));

  scoped_refptr<VideoFrame> frame_3 = GetFrame(20);
  EXPECT_NE(pool_->UnwrapFrame(*frame_1), pool_->UnwrapFrame(*frame_3));
  EXPECT_FALSE(frame_1->IsSameDmaBufsAs(*frame_3));
}

}  // namespace media
