// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_ENCODER_VIDEO_ENCODER_TEST_ENVIRONMENT_H_
#define MEDIA_GPU_TEST_VIDEO_ENCODER_VIDEO_ENCODER_TEST_ENVIRONMENT_H_

#include <limits>
#include <memory>

#include "base/files/file_path.h"
#include "media/gpu/test/video_test_environment.h"

namespace gpu {
class GpuMemoryBufferFactory;
}

namespace media {
namespace test {

class Video;

// Test environment for video encoder tests. Performs setup and teardown once
// for the entire test run.
class VideoEncoderTestEnvironment : public VideoTestEnvironment {
 public:
  static VideoEncoderTestEnvironment* Create(
      const base::FilePath& video_path,
      const base::FilePath& video_metadata_path,
      const base::FilePath& output_folder);
  ~VideoEncoderTestEnvironment() override;

  // Get the video the tests will be ran on.
  const media::test::Video* Video() const;
  // Get the output folder.
  const base::FilePath& OutputFolder() const;

  // Get the GpuMemoryBufferFactory for doing buffer allocations. This needs to
  // survive as long as the process is alive just like in production which is
  // why it's in here as there are threads that won't immediately die when an
  // individual test is completed.
  gpu::GpuMemoryBufferFactory* GetGpuMemoryBufferFactory() const;

 private:
  VideoEncoderTestEnvironment(std::unique_ptr<media::test::Video> video,
                              const base::FilePath& output_folder);

  // Video file to be used for testing.
  const std::unique_ptr<media::test::Video> video_;
  // Output folder to be used to store test artifacts (e.g. perf metrics).
  const base::FilePath output_folder_;

  std::unique_ptr<gpu::GpuMemoryBufferFactory> gpu_memory_buffer_factory_;
};
}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_VIDEO_ENCODER_VIDEO_ENCODER_TEST_ENVIRONMENT_H_
