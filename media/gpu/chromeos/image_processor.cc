// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/image_processor.h"

#include <ostream>
#include <sstream>

#include "base/strings/stringprintf.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/gpu/macros.h"

namespace media {

namespace {

std::ostream& operator<<(std::ostream& ostream,
                         const VideoFrame::StorageType& storage_type) {
  ostream << VideoFrame::StorageTypeToString(storage_type);
  return ostream;
}

template <class T>
std::string VectorToString(const std::vector<T>& vec) {
  std::ostringstream result;
  std::string delim;
  result << "[";
  for (const T& v : vec) {
    result << delim << v;
    if (delim.size() == 0)
      delim = ", ";
  }
  result << "]";
  return result.str();
}

// Verify if the format of |frame| matches |config|.
bool CheckVideoFrameFormat(const ImageProcessor::PortConfig& config,
                           const VideoFrame& frame) {
  // Because propriatary format fourcc will map to other common VideoPixelFormat
  // with same layout, we convert to VideoPixelFormat to check.
  if (frame.format() != config.fourcc.ToVideoPixelFormat()) {
    VLOGF(1) << "Invalid frame format="
             << VideoPixelFormatToString(frame.format())
             << ", expected=" << config.fourcc.ToString();
    return false;
  }

  if (frame.layout().coded_size() != config.size) {
    VLOGF(1) << "Invalid frame size=" << frame.layout().coded_size().ToString()
             << ", expected=" << config.size.ToString();
    return false;
  }

  if (frame.storage_type() != config.storage_type()) {
    VLOGF(1) << "Invalid frame.storage_type=" << frame.storage_type()
             << ", input_storage_type=" << config.storage_type();
    return false;
  }

  return true;
}

}  // namespace

ImageProcessor::PortConfig::PortConfig(const PortConfig&) = default;

ImageProcessor::PortConfig::PortConfig(
    Fourcc fourcc,
    const gfx::Size& size,
    const std::vector<ColorPlaneLayout>& planes,
    const gfx::Size& visible_size,
    const std::vector<VideoFrame::StorageType>& preferred_storage_types)
    : fourcc(fourcc),
      size(size),
      planes(planes),
      visible_size(visible_size),
      preferred_storage_types(preferred_storage_types) {}

ImageProcessor::PortConfig::~PortConfig() = default;

std::string ImageProcessor::PortConfig::ToString() const {
  return base::StringPrintf(
      "PortConfig(format:%s, size:%s, planes: %s, visible_size:%s, "
      "storage_types:%s)",
      fourcc.ToString().c_str(), size.ToString().c_str(),
      VectorToString(planes).c_str(), visible_size.ToString().c_str(),
      VectorToString(preferred_storage_types).c_str());
}

ImageProcessor::ImageProcessor(
    const ImageProcessor::PortConfig& input_config,
    const ImageProcessor::PortConfig& output_config,
    OutputMode output_mode,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner)
    : input_config_(input_config),
      output_config_(output_config),
      output_mode_(output_mode),
      client_task_runner_(std::move(client_task_runner)) {
  DETACH_FROM_SEQUENCE(client_sequence_checker_);
}

ImageProcessor::~ImageProcessor() = default;

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
bool ImageProcessor::Process(scoped_refptr<VideoFrame> frame,
                             LegacyFrameReadyCB cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DCHECK_EQ(output_mode(), OutputMode::ALLOCATE);

  return ProcessInternal(std::move(frame), std::move(cb));
}

bool ImageProcessor::ProcessInternal(scoped_refptr<VideoFrame> frame,
                                     LegacyFrameReadyCB cb) {
  NOTIMPLEMENTED();
  return false;
}
#endif

bool ImageProcessor::Process(scoped_refptr<VideoFrame> input_frame,
                             scoped_refptr<VideoFrame> output_frame,
                             FrameReadyCB cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DCHECK_EQ(output_mode(), OutputMode::IMPORT);
  DCHECK(input_frame);
  DCHECK(output_frame);

  if (!CheckVideoFrameFormat(input_config_, *input_frame) ||
      !CheckVideoFrameFormat(output_config_, *output_frame))
    return false;

  return ProcessInternal(std::move(input_frame), std::move(output_frame),
                         std::move(cb));
}

}  // namespace media
