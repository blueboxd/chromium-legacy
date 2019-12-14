// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

namespace {

class VideoFrameTest : public testing::Test {
 public:
  VideoFrame* CreateBlinkVideoFrame(
      scoped_refptr<media::VideoFrame> media_frame) {
    return MakeGarbageCollected<VideoFrame>(std::move(media_frame));
  }
  scoped_refptr<media::VideoFrame> CreateBlackMediaVideoFrame(
      base::TimeDelta timestamp,
      media::VideoPixelFormat format,
      const gfx::Size& coded_size,
      const gfx::Size& visible_size) {
    scoped_refptr<media::VideoFrame> media_frame =
        media::VideoFrame::WrapVideoFrame(
            media::VideoFrame::CreateBlackFrame(coded_size), format,
            gfx::Rect(visible_size) /* visible_rect */,
            visible_size /* natural_size */);
    media_frame->set_timestamp(timestamp);
    return media_frame;
  }
};

TEST_F(VideoFrameTest, ConstructorAndAttributes) {
  scoped_refptr<media::VideoFrame> media_frame = CreateBlackMediaVideoFrame(
      base::TimeDelta::FromMicroseconds(1000), media::PIXEL_FORMAT_I420,
      gfx::Size(112, 208) /* coded_size */,
      gfx::Size(100, 200) /* visible_size */);
  VideoFrame* blink_frame = CreateBlinkVideoFrame(media_frame);

  EXPECT_EQ(1000u, blink_frame->timestamp());
  EXPECT_EQ(112u, blink_frame->coded_width());
  EXPECT_EQ(208u, blink_frame->coded_height());
  EXPECT_EQ(100u, blink_frame->visible_width());
  EXPECT_EQ(200u, blink_frame->visible_height());
  EXPECT_EQ(media_frame, blink_frame->frame());

  blink_frame->release();

  EXPECT_EQ(0u, blink_frame->timestamp());
  EXPECT_EQ(0u, blink_frame->coded_width());
  EXPECT_EQ(0u, blink_frame->coded_height());
  EXPECT_EQ(0u, blink_frame->visible_width());
  EXPECT_EQ(0u, blink_frame->visible_height());
  EXPECT_EQ(nullptr, blink_frame->frame());
}

}  // namespace

}  // namespace blink
