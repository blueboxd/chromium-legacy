// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/shared_memory_buffer_tracker.h"

#include "base/logging.h"
#include "ui/gfx/geometry/size.h"

namespace {

size_t CalculateRequiredBufferSize(
    const gfx::Size& dimensions,
    media::VideoPixelFormat format,
    const media::mojom::PlaneStridesPtr& strides) {
  if (strides) {
    size_t result = 0u;
    for (size_t plane_index = 0;
         plane_index < media::VideoFrame::NumPlanes(format); plane_index++) {
      result +=
          strides->stride_by_plane[plane_index] *
          media::VideoFrame::Rows(plane_index, format, dimensions.height());
    }
    return result;
  } else {
    return media::VideoCaptureFormat(dimensions, 0.0f, format)
        .ImageAllocationSize();
  }
}

}  // namespace

namespace media {

SharedMemoryBufferTracker::SharedMemoryBufferTracker() = default;

SharedMemoryBufferTracker::~SharedMemoryBufferTracker() = default;

bool SharedMemoryBufferTracker::Init(const gfx::Size& dimensions,
                                     VideoPixelFormat format,
                                     const mojom::PlaneStridesPtr& strides) {
  const size_t buffer_size =
      CalculateRequiredBufferSize(dimensions, format, strides);
  return provider_.InitForSize(buffer_size);
}

bool SharedMemoryBufferTracker::IsReusableForFormat(
    const gfx::Size& dimensions,
    VideoPixelFormat format,
    const mojom::PlaneStridesPtr& strides) {
  return GetMemorySizeInBytes() >=
         CalculateRequiredBufferSize(dimensions, format, strides);
}

std::unique_ptr<VideoCaptureBufferHandle>
SharedMemoryBufferTracker::GetMemoryMappedAccess() {
  return provider_.GetHandleForInProcessAccess();
}

mojo::ScopedSharedBufferHandle SharedMemoryBufferTracker::GetHandleForTransit(
    bool read_only) {
  return provider_.GetHandleForInterProcessTransit(read_only);
}

base::SharedMemoryHandle
SharedMemoryBufferTracker::GetNonOwnedSharedMemoryHandleForLegacyIPC() {
  return provider_.GetNonOwnedSharedMemoryHandleForLegacyIPC();
}

#if defined(OS_CHROMEOS)
gfx::GpuMemoryBufferHandle
SharedMemoryBufferTracker::GetGpuMemoryBufferHandle() {
  return provider_.GetGpuMemoryBufferHandle();
}
#endif

uint32_t SharedMemoryBufferTracker::GetMemorySizeInBytes() {
  return provider_.GetMemorySizeInBytes();
}

}  // namespace media
