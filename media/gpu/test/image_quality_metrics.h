// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_IMAGE_QUALITY_METRICS_H_
#define MEDIA_GPU_TEST_IMAGE_QUALITY_METRICS_H_

#include <stdint.h>

namespace media {

class VideoFrame;

namespace test {

// Compare each byte of two VideoFrames, |frame1| and |frame2|, allowing an
// error up to |tolerance|. Return the number of bytes of which the difference
// is more than |tolerance|.
size_t CompareFramesWithErrorDiff(const VideoFrame& frame1,
                                  const VideoFrame& frame2,
                                  uint8_t tolerance);
}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_IMAGE_QUALITY_METRICS_H_
