// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>
#include <utility>

#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"

#define ASSERT_TRUE_OR_RETURN(predicate, return_value) \
  do {                                                 \
    if (!(predicate)) {                                \
      ADD_FAILURE();                                   \
      return (return_value);                           \
    }                                                  \
  } while (0)

namespace media {
namespace test {

size_t CompareFramesWithErrorDiff(const VideoFrame& frame1,
                                  const VideoFrame& frame2,
                                  uint8_t tolerance) {
  ASSERT_TRUE_OR_RETURN(frame1.format() == frame2.format(),
                        std::numeric_limits<std::size_t>::max());
  ASSERT_TRUE_OR_RETURN(frame1.visible_rect() == frame2.visible_rect(),
                        std::numeric_limits<std::size_t>::max());
  ASSERT_TRUE_OR_RETURN(frame1.visible_rect().origin() == gfx::Point(0, 0),
                        std::numeric_limits<std::size_t>::max());
  ASSERT_TRUE_OR_RETURN(frame1.IsMappable() && frame2.IsMappable(),
                        std::numeric_limits<std::size_t>::max());
  size_t diff_cnt = 0;

  const VideoPixelFormat format = frame1.format();
  const size_t num_planes = VideoFrame::NumPlanes(format);
  const gfx::Size& visible_size = frame1.visible_rect().size();
  for (size_t i = 0; i < num_planes; ++i) {
    const uint8_t* data1 = frame1.data(i);
    const int stride1 = frame1.stride(i);
    const uint8_t* data2 = frame2.data(i);
    const int stride2 = frame2.stride(i);
    const size_t rows = VideoFrame::Rows(i, format, visible_size.height());
    const int row_bytes = VideoFrame::RowBytes(i, format, visible_size.width());
    for (size_t r = 0; r < rows; ++r) {
      for (int c = 0; c < row_bytes; c++) {
        uint8_t b1 = data1[(stride1 * r) + c];
        uint8_t b2 = data2[(stride2 * r) + c];
        uint8_t diff = std::max(b1, b2) - std::min(b1, b2);
        diff_cnt += diff > tolerance;
      }
    }
  }
  return diff_cnt;
}

}  // namespace test
}  // namespace media
