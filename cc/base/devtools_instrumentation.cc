// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/devtools_instrumentation.h"

namespace cc {
namespace devtools_instrumentation {

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
  if (image_type_ == ImageType::kWebP) {
    switch (decode_type_) {
      case kSoftware:
        UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
            "Renderer4.ImageDecodeTaskDurationUs.WebP.Software", duration, min,
            max, bucket_count);
        break;
      case kGpu:
        UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
            "Renderer4.ImageDecodeTaskDurationUs.WebP.Gpu", duration, min, max,
            bucket_count);
        break;
    }
  }
  switch (task_type_) {
    case kInRaster:
      switch (decode_type_) {
        case kSoftware:
          UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
              "Renderer4.ImageDecodeTaskDurationUs.Software", duration, min,
              max, bucket_count);
          break;
        case kGpu:
          UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
              "Renderer4.ImageDecodeTaskDurationUs.Gpu", duration, min, max,
              bucket_count);
          break;
      }
      break;
    case kOutOfRaster:
      switch (decode_type_) {
        case kSoftware:
          UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
              "Renderer4.ImageDecodeTaskDurationUs.OutOfRaster.Software",
              duration, min, max, bucket_count);
          break;
        case kGpu:
          UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
              "Renderer4.ImageDecodeTaskDurationUs.OutOfRaster.Gpu", duration,
              min, max, bucket_count);
          break;
      }
      break;
  }
}

}  // namespace devtools_instrumentation
}  // namespace cc
