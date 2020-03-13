// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_frame_validator.h"

#include "base/bind.h"
#include "base/files/file.h"
#include "base/hash/md5.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "media/base/video_frame.h"
#include "media/gpu/macros.h"
#include "media/gpu/test/image_quality_metrics.h"
#include "media/gpu/test/video_test_helpers.h"
#include "media/gpu/video_frame_mapper.h"
#include "media/gpu/video_frame_mapper_factory.h"
#include "media/media_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace test {

VideoFrameValidator::VideoFrameValidator(
    std::unique_ptr<VideoFrameProcessor> corrupt_frame_processor)
    : corrupt_frame_processor_(std::move(corrupt_frame_processor)),
      num_frames_validating_(0),
      frame_validator_thread_("FrameValidatorThread"),
      frame_validator_cv_(&frame_validator_lock_) {
  DETACH_FROM_SEQUENCE(validator_sequence_checker_);
  DETACH_FROM_SEQUENCE(validator_thread_sequence_checker_);
}

VideoFrameValidator::~VideoFrameValidator() {
  Destroy();
}

bool VideoFrameValidator::Initialize() {
  if (!frame_validator_thread_.Start()) {
    LOG(ERROR) << "Failed to start frame validator thread";
    return false;
  }
  return true;
}

void VideoFrameValidator::Destroy() {
  frame_validator_thread_.Stop();
  base::AutoLock auto_lock(frame_validator_lock_);
  DCHECK_EQ(0u, num_frames_validating_);
}

size_t VideoFrameValidator::GetMismatchedFramesCount() const {
  base::AutoLock auto_lock(frame_validator_lock_);
  return mismatched_frames_.size();
}

void VideoFrameValidator::ProcessVideoFrame(
    scoped_refptr<const VideoFrame> video_frame,
    size_t frame_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(validator_sequence_checker_);

  if (!video_frame) {
    LOG(ERROR) << "Video frame is nullptr";
    return;
  }

  if (video_frame->visible_rect().IsEmpty()) {
    // This occurs in bitstream buffer in webrtc scenario.
    DLOG(WARNING) << "Skipping validation, frame_index=" << frame_index
                  << " because visible_rect is empty";
    return;
  }

  base::AutoLock auto_lock(frame_validator_lock_);
  num_frames_validating_++;

  // Unretained is safe here, as we should not destroy the validator while there
  // are still frames being validated.
  frame_validator_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoFrameValidator::ProcessVideoFrameTask,
                     base::Unretained(this), video_frame, frame_index));
}

bool VideoFrameValidator::WaitUntilDone() {
  base::AutoLock auto_lock(frame_validator_lock_);
  while (num_frames_validating_ > 0) {
    frame_validator_cv_.Wait();
  }

  if (corrupt_frame_processor_ && !corrupt_frame_processor_->WaitUntilDone())
    return false;

  if (mismatched_frames_.size() > 0u) {
    LOG(ERROR) << mismatched_frames_.size() << " frames failed to validate.";
    return false;
  }
  return true;
}

void VideoFrameValidator::ProcessVideoFrameTask(
    const scoped_refptr<const VideoFrame> video_frame,
    size_t frame_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(validator_thread_sequence_checker_);

  scoped_refptr<const VideoFrame> frame = video_frame;
  // If this is a DMABuf-backed memory frame we need to map it before accessing.
#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
  if (frame->storage_type() == VideoFrame::STORAGE_DMABUFS ||
      frame->storage_type() == VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
    // Create VideoFrameMapper if not yet created. The decoder's output pixel
    // format is not known yet when creating the VideoFrameValidator. We can
    // only create the VideoFrameMapper upon receiving the first video frame.
    if (!video_frame_mapper_) {
      video_frame_mapper_ = VideoFrameMapperFactory::CreateMapper(
          video_frame->format(), video_frame->storage_type());
      ASSERT_TRUE(video_frame_mapper_) << "Failed to create VideoFrameMapper";
    }

    frame = video_frame_mapper_->Map(std::move(frame));
    if (!frame) {
      LOG(ERROR) << "Failed to map video frame";
      return;
    }
  }
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

  ASSERT_TRUE(frame->IsMappable());

  auto mismatched_info = Validate(frame, frame_index);

  base::AutoLock auto_lock(frame_validator_lock_);
  if (mismatched_info) {
    mismatched_frames_.push_back(std::move(mismatched_info));
    // Perform additional processing on the corrupt video frame if requested.
    if (corrupt_frame_processor_)
      corrupt_frame_processor_->ProcessVideoFrame(frame, frame_index);
  }

  num_frames_validating_--;
  frame_validator_cv_.Signal();
}

struct MD5VideoFrameValidator::MD5MismatchedFrameInfo
    : public VideoFrameValidator::MismatchedFrameInfo {
  MD5MismatchedFrameInfo(size_t frame_index,
                         const std::string& computed_md5,
                         const std::string& expected_md5)
      : MismatchedFrameInfo(frame_index),
        computed_md5(computed_md5),
        expected_md5(expected_md5) {}
  ~MD5MismatchedFrameInfo() override = default;
  void Print() const override {
    LOG(ERROR) << "frame_index: " << frame_index
               << ", computed_md5: " << computed_md5
               << ", expected_md5: " << expected_md5;
  }

  const std::string computed_md5;
  const std::string expected_md5;
};

// static
std::unique_ptr<MD5VideoFrameValidator> MD5VideoFrameValidator::Create(
    const std::vector<std::string>& expected_frame_checksums,
    VideoPixelFormat validation_format,
    std::unique_ptr<VideoFrameProcessor> corrupt_frame_processor) {
  auto video_frame_validator = base::WrapUnique(
      new MD5VideoFrameValidator(expected_frame_checksums, validation_format,
                                 std::move(corrupt_frame_processor)));
  if (!video_frame_validator->Initialize()) {
    LOG(ERROR) << "Failed to initialize MD5VideoFrameValidator.";
    return nullptr;
  }

  return video_frame_validator;
}

MD5VideoFrameValidator::MD5VideoFrameValidator(
    const std::vector<std::string>& expected_frame_checksums,
    VideoPixelFormat validation_format,
    std::unique_ptr<VideoFrameProcessor> corrupt_frame_processor)
    : VideoFrameValidator(std::move(corrupt_frame_processor)),
      expected_frame_checksums_(expected_frame_checksums),
      validation_format_(validation_format) {}

MD5VideoFrameValidator::~MD5VideoFrameValidator() = default;

std::unique_ptr<VideoFrameValidator::MismatchedFrameInfo>
MD5VideoFrameValidator::Validate(scoped_refptr<const VideoFrame> frame,
                                 size_t frame_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(validator_thread_sequence_checker_);
  if (frame->format() != validation_format_) {
    frame = ConvertVideoFrame(frame.get(), validation_format_);
  }
  CHECK(frame);

  std::string computed_md5 = ComputeMD5FromVideoFrame(*frame);
  if (expected_frame_checksums_.size() > 0) {
    LOG_IF(FATAL, frame_index >= expected_frame_checksums_.size())
        << "Frame number is over than the number of read md5 values in file.";
    const auto& expected_md5 = expected_frame_checksums_[frame_index];
    if (computed_md5 != expected_md5)
      return std::make_unique<MD5MismatchedFrameInfo>(frame_index, computed_md5,
                                                      expected_md5);
  }
  return nullptr;
}

std::string MD5VideoFrameValidator::ComputeMD5FromVideoFrame(
    const VideoFrame& video_frame) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(validator_thread_sequence_checker_);
  base::MD5Context context;
  base::MD5Init(&context);
  VideoFrame::HashFrameForTesting(&context, video_frame);
  base::MD5Digest digest;
  base::MD5Final(&digest, &context);
  return MD5DigestToBase16(digest);
}

struct RawVideoFrameValidator::RawMismatchedFrameInfo
    : public VideoFrameValidator::MismatchedFrameInfo {
  RawMismatchedFrameInfo(size_t frame_index, size_t diff_cnt)
      : MismatchedFrameInfo(frame_index), diff_cnt(diff_cnt) {}
  ~RawMismatchedFrameInfo() override = default;
  void Print() const override {
    LOG(ERROR) << "frame_index: " << frame_index << ", diff_cnt: " << diff_cnt;
  }

  size_t diff_cnt;
};

// static
std::unique_ptr<RawVideoFrameValidator> RawVideoFrameValidator::Create(
    const GetModelFrameCB& get_model_frame_cb,
    uint8_t tolerance,
    std::unique_ptr<VideoFrameProcessor> corrupt_frame_processor) {
  auto video_frame_validator = base::WrapUnique(new RawVideoFrameValidator(
      get_model_frame_cb, tolerance, std::move(corrupt_frame_processor)));
  if (!video_frame_validator->Initialize()) {
    LOG(ERROR) << "Failed to initialize RawVideoFrameValidator.";
    return nullptr;
  }

  return video_frame_validator;
}

RawVideoFrameValidator::RawVideoFrameValidator(
    const GetModelFrameCB& get_model_frame_cb,
    uint8_t tolerance,
    std::unique_ptr<VideoFrameProcessor> corrupt_frame_processor)
    : VideoFrameValidator(std::move(corrupt_frame_processor)),
      get_model_frame_cb_(get_model_frame_cb),
      tolerance_(tolerance) {}

RawVideoFrameValidator::~RawVideoFrameValidator() = default;

std::unique_ptr<VideoFrameValidator::MismatchedFrameInfo>
RawVideoFrameValidator::Validate(scoped_refptr<const VideoFrame> frame,
                                 size_t frame_index) {
  SEQUENCE_CHECKER(validator_thread_sequence_checker_);
  auto model_frame = get_model_frame_cb_.Run(frame_index);
  CHECK(model_frame);
  size_t diff_cnt =
      CompareFramesWithErrorDiff(*frame, *model_frame, tolerance_);
  if (diff_cnt > 0)
    return std::make_unique<RawMismatchedFrameInfo>(frame_index, diff_cnt);
  return nullptr;
}

struct PSNRVideoFrameValidator::PSNRMismatchedFrameInfo
    : public VideoFrameValidator::MismatchedFrameInfo {
  PSNRMismatchedFrameInfo(size_t frame_index, double psnr)
      : MismatchedFrameInfo(frame_index), psnr(psnr) {}
  ~PSNRMismatchedFrameInfo() override = default;
  void Print() const override {
    LOG(ERROR) << "frame_index: " << frame_index << ", psnr: " << psnr;
  }

  double psnr;
};

// static
std::unique_ptr<PSNRVideoFrameValidator> PSNRVideoFrameValidator::Create(
    const GetModelFrameCB& get_model_frame_cb,
    double tolerance,
    std::unique_ptr<VideoFrameProcessor> corrupt_frame_processor) {
  auto video_frame_validator = base::WrapUnique(new PSNRVideoFrameValidator(
      get_model_frame_cb, tolerance, std::move(corrupt_frame_processor)));
  if (!video_frame_validator->Initialize()) {
    LOG(ERROR) << "Failed to initialize PSNRVideoFrameValidator.";
    return nullptr;
  }

  return video_frame_validator;
}

PSNRVideoFrameValidator::PSNRVideoFrameValidator(
    const GetModelFrameCB& get_model_frame_cb,
    double tolerance,
    std::unique_ptr<VideoFrameProcessor> corrupt_frame_processor)
    : VideoFrameValidator(std::move(corrupt_frame_processor)),
      get_model_frame_cb_(get_model_frame_cb),
      tolerance_(tolerance) {}

PSNRVideoFrameValidator::~PSNRVideoFrameValidator() = default;

std::unique_ptr<VideoFrameValidator::MismatchedFrameInfo>
PSNRVideoFrameValidator::Validate(scoped_refptr<const VideoFrame> frame,
                                  size_t frame_index) {
  SEQUENCE_CHECKER(validator_thread_sequence_checker_);
  auto model_frame = get_model_frame_cb_.Run(frame_index);
  CHECK(model_frame);
  double psnr = ComputePSNR(*frame, *model_frame);
  DVLOGF(4) << "frame_index: " << frame_index << ", psnr: " << psnr;
  if (psnr < tolerance_)
    return std::make_unique<PSNRMismatchedFrameInfo>(frame_index, psnr);
  return nullptr;
}

struct SSIMVideoFrameValidator::SSIMMismatchedFrameInfo
    : public VideoFrameValidator::MismatchedFrameInfo {
  SSIMMismatchedFrameInfo(size_t frame_index, double ssim)
      : MismatchedFrameInfo(frame_index), ssim(ssim) {}
  ~SSIMMismatchedFrameInfo() override = default;
  void Print() const override {
    LOG(ERROR) << "frame_index: " << frame_index << ", ssim: " << ssim;
  }

  double ssim;
};

// static
std::unique_ptr<SSIMVideoFrameValidator> SSIMVideoFrameValidator::Create(
    const GetModelFrameCB& get_model_frame_cb,
    double tolerance,
    std::unique_ptr<VideoFrameProcessor> corrupt_frame_processor) {
  auto video_frame_validator = base::WrapUnique(new SSIMVideoFrameValidator(
      get_model_frame_cb, tolerance, std::move(corrupt_frame_processor)));
  if (!video_frame_validator->Initialize()) {
    LOG(ERROR) << "Failed to initialize SSIMVideoFrameValidator.";
    return nullptr;
  }

  return video_frame_validator;
}

SSIMVideoFrameValidator::SSIMVideoFrameValidator(
    const GetModelFrameCB& get_model_frame_cb,
    double tolerance,
    std::unique_ptr<VideoFrameProcessor> corrupt_frame_processor)
    : VideoFrameValidator(std::move(corrupt_frame_processor)),
      get_model_frame_cb_(get_model_frame_cb),
      tolerance_(tolerance) {}

SSIMVideoFrameValidator::~SSIMVideoFrameValidator() = default;

std::unique_ptr<VideoFrameValidator::MismatchedFrameInfo>
SSIMVideoFrameValidator::Validate(scoped_refptr<const VideoFrame> frame,
                                  size_t frame_index) {
  SEQUENCE_CHECKER(validator_thread_sequence_checker_);
  auto model_frame = get_model_frame_cb_.Run(frame_index);
  CHECK(model_frame);
  double ssim = ComputeSSIM(*frame, *model_frame);
  DVLOGF(4) << "frame_index: " << frame_index << ", ssim: " << ssim;
  if (ssim < tolerance_)
    return std::make_unique<SSIMMismatchedFrameInfo>(frame_index, ssim);
  return nullptr;
}
}  // namespace test
}  // namespace media
