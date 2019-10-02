// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_PLAYER_VIDEO_PLAYER_TEST_ENVIRONMENT_H_
#define MEDIA_GPU_TEST_VIDEO_PLAYER_VIDEO_PLAYER_TEST_ENVIRONMENT_H_

#include <memory>

#include "base/files/file_path.h"
#include "media/gpu/test/video_test_environment.h"

namespace media {
namespace test {

class Video;

// The frame output mode allows controlling which video frames are written to
// disk. Writing frames will greatly slow down the test and generate a lot of
// test artifacts, so be careful when configuring other modes than kNone in
// automated testing.
enum class FrameOutputMode {
  kNone,     // Don't output any frames.
  kCorrupt,  // Only output corrupt frames.
  kAll       // Output all frames.
};

// Test environment for video decode tests. Performs setup and teardown once for
// the entire test run.
class VideoPlayerTestEnvironment : public VideoTestEnvironment {
 public:
  static VideoPlayerTestEnvironment* Create(
      const base::FilePath& video_path,
      const base::FilePath& video_metadata_path,
      bool enable_validator,
      FrameOutputMode frame_output_mode,
      const base::FilePath& output_folder,
      bool use_vd);
  ~VideoPlayerTestEnvironment() override;

  // Set up video test environment, called once for entire test run.
  void SetUp() override;

  // Get the video the tests will be ran on.
  const media::test::Video* Video() const;
  // Check whether frame validation is enabled.
  bool IsValidatorEnabled() const;
  // Get the frame output mode.
  FrameOutputMode GetFrameOutputMode() const;
  // Get the output folder.
  const base::FilePath& OutputFolder() const;
  // Check whether we should use VD-based video decoders instead of VDA-based.
  bool UseVD() const;
  // Whether import mode is supported, valid after SetUp() has been called.
  bool ImportSupported() const;

 private:
  VideoPlayerTestEnvironment(std::unique_ptr<media::test::Video> video,
                             bool enable_validator,
                             FrameOutputMode frame_output_mode,
                             const base::FilePath& output_folder,
                             bool use_vd);

  const std::unique_ptr<media::test::Video> video_;
  const bool enable_validator_;
  const FrameOutputMode frame_output_mode_;
  const base::FilePath output_folder_;
  const bool use_vd_;
  // TODO(dstaessens): Remove this once all allocate-only platforms reached EOL.
  bool import_supported_ = false;
};
}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_VIDEO_PLAYER_VIDEO_PLAYER_TEST_ENVIRONMENT_H_
