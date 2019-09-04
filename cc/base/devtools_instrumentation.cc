// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/devtools_instrumentation.h"

namespace cc {
namespace devtools_instrumentation {
namespace {

void RecordMicrosecondTimesUmaByDecodeType(
    const std::string& metric_prefix,
    base::TimeDelta duration,
    base::TimeDelta min,
    base::TimeDelta max,
    uint32_t bucket_count,
    ScopedImageDecodeTask::DecodeType decode_type_) {
  switch (decode_type_) {
    case ScopedImageDecodeTask::kSoftware:
      UmaHistogramCustomMicrosecondsTimes(metric_prefix + ".Software", duration,
                                          min, max, bucket_count);
      break;
    case ScopedImageDecodeTask::kGpu:
      UmaHistogramCustomMicrosecondsTimes(metric_prefix + ".Gpu", duration, min,
                                          max, bucket_count);
      break;
  }
}
}  // namespace

namespace internal {
constexpr const char CategoryName::CategoryName::kTimeline[];
constexpr const char CategoryName::CategoryName::kTimelineFrame[];
const char kData[] = "data";
const char kFrameId[] = "frameId";
const char kLayerId[] = "layerId";
const char kLayerTreeId[] = "layerTreeId";
const char kPixelRefId[] = "pixelRefId";

const char kImageDecodeTask[] = "ImageDecodeTask";
const char kBeginFrame[] = "BeginFrame";
const char kNeedsBeginFrameChanged[] = "NeedsBeginFrameChanged";
const char kActivateLayerTree[] = "ActivateLayerTree";
const char kRequestMainThreadFrame[] = "RequestMainThreadFrame";
const char kBeginMainThreadFrame[] = "BeginMainThreadFrame";
const char kDrawFrame[] = "DrawFrame";
const char kCompositeLayers[] = "CompositeLayers";
}  // namespace internal

const char kPaintSetup[] = "PaintSetup";
const char kUpdateLayer[] = "UpdateLayer";

ScopedImageDecodeTask::ScopedImageDecodeTask(const void* image_ptr,
                                             DecodeType decode_type,
                                             TaskType task_type,
                                             ImageType image_type)
    : decode_type_(decode_type),
      task_type_(task_type),
      start_time_(base::TimeTicks::Now()),
      image_type_(image_type) {
  TRACE_EVENT_BEGIN1(internal::CategoryName::kTimeline,
                     internal::kImageDecodeTask, internal::kPixelRefId,
                     reinterpret_cast<uint64_t>(image_ptr));
}

ScopedImageDecodeTask::~ScopedImageDecodeTask() {
  TRACE_EVENT_END0(internal::CategoryName::kTimeline,
                   internal::kImageDecodeTask);
  if (suppress_metrics_)
    return;

  const uint32_t bucket_count = 50;
  base::TimeDelta min = base::TimeDelta::FromMicroseconds(1);
  base::TimeDelta max = base::TimeDelta::FromMilliseconds(1000);
  auto duration = base::TimeTicks::Now() - start_time_;
  switch (image_type_) {
    case ImageType::kWebP:
      RecordMicrosecondTimesUmaByDecodeType(
          "Renderer4.ImageDecodeTaskDurationUs.WebP", duration, min, max,
          bucket_count, decode_type_);
      break;
    case ImageType::kJpeg:
      RecordMicrosecondTimesUmaByDecodeType(
          "Renderer4.ImageDecodeTaskDurationUs.Jpeg", duration, min, max,
          bucket_count, decode_type_);
      break;
    case ImageType::kOther:
      RecordMicrosecondTimesUmaByDecodeType(
          "Renderer4.ImageDecodeTaskDurationUs.Other", duration, min, max,
          bucket_count, decode_type_);
      break;
  }
  switch (task_type_) {
    case kInRaster:
      RecordMicrosecondTimesUmaByDecodeType(
          "Renderer4.ImageDecodeTaskDurationUs", duration, min, max,
          bucket_count, decode_type_);
      break;
    case kOutOfRaster:
      RecordMicrosecondTimesUmaByDecodeType(
          "Renderer4.ImageDecodeTaskDurationUs.OutOfRaster", duration, min, max,
          bucket_count, decode_type_);
      break;
  }
}

}  // namespace devtools_instrumentation
}  // namespace cc
