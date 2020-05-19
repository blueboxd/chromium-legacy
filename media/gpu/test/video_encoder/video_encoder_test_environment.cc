// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_encoder/video_encoder_test_environment.h"

#include <algorithm>
#include <utility>

#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "media/gpu/macros.h"
#include "media/gpu/test/video.h"

namespace media {
namespace test {
namespace {
struct CodecParamToProfile {
  const char* codec;
  const VideoCodecProfile profile;
} kCodecParamToProfile[] = {
    {"h264baseline", H264PROFILE_BASELINE}, {"h264", H264PROFILE_MAIN},
    {"h264main", H264PROFILE_MAIN},         {"vp8", VP8PROFILE_ANY},
    {"vp9", VP9PROFILE_PROFILE0},
};
}  // namespace

// static
VideoEncoderTestEnvironment* VideoEncoderTestEnvironment::Create(
    const base::FilePath& video_path,
    const base::FilePath& video_metadata_path,
    const base::FilePath& output_folder,
    const std::string& codec) {
  if (video_path.empty()) {
    LOG(ERROR) << "No video specified";
    return nullptr;
  }
  auto video =
      std::make_unique<media::test::Video>(video_path, video_metadata_path);
  if (!video->Load()) {
    LOG(ERROR) << "Failed to load " << video_path;
    return nullptr;
  }

  // If the video file has the .webm format it needs to be decoded first.
  // TODO(b/151134705): Add support to cache decompressed video files.
  if (video->FilePath().MatchesExtension(FILE_PATH_LITERAL(".webm"))) {
    VLOGF(1) << "Test video " << video->FilePath()
             << " is compressed, decoding...";
    if (!video->Decode()) {
      LOG(ERROR) << "Failed to decode " << video->FilePath();
      return nullptr;
    }
  }

  if (video->PixelFormat() == VideoPixelFormat::PIXEL_FORMAT_UNKNOWN) {
    LOG(ERROR) << "Test video " << video->FilePath()
               << " has an invalid video pixel format "
               << VideoPixelFormatToString(video->PixelFormat());
    return nullptr;
  }

  const auto* it = std::find_if(
      std::begin(kCodecParamToProfile), std::end(kCodecParamToProfile),
      [codec](const auto& cp) { return cp.codec == codec; });
  if (it == std::end(kCodecParamToProfile)) {
    LOG(ERROR) << "Unknown codec: " << codec;
    return nullptr;
  }
  VideoCodecProfile profile = it->profile;
  return new VideoEncoderTestEnvironment(std::move(video), output_folder,
                                         profile);
}

VideoEncoderTestEnvironment::VideoEncoderTestEnvironment(
    std::unique_ptr<media::test::Video> video,
    const base::FilePath& output_folder,
    VideoCodecProfile profile)
    : video_(std::move(video)),
      output_folder_(output_folder),
      profile_(profile),
      gpu_memory_buffer_factory_(
          gpu::GpuMemoryBufferFactory::CreateNativeType(nullptr)) {}

VideoEncoderTestEnvironment::~VideoEncoderTestEnvironment() = default;

const media::test::Video* VideoEncoderTestEnvironment::Video() const {
  return video_.get();
}

const base::FilePath& VideoEncoderTestEnvironment::OutputFolder() const {
  return output_folder_;
}

VideoCodecProfile VideoEncoderTestEnvironment::Profile() const {
  return profile_;
}

gpu::GpuMemoryBufferFactory*
VideoEncoderTestEnvironment::GetGpuMemoryBufferFactory() const {
  return gpu_memory_buffer_factory_.get();
}

}  // namespace test
}  // namespace media
