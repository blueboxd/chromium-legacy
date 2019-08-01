// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/video_accelerator/arc_video_accelerator_util.h"

#include <sys/types.h>
#include <unistd.h>

#include "base/files/platform_file.h"
#include "base/numerics/checked_math.h"
#include "media/base/video_frame.h"
#include "media/gpu/macros.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace arc {

base::ScopedFD UnwrapFdFromMojoHandle(mojo::ScopedHandle handle) {
  if (!handle.is_valid()) {
    VLOGF(1) << "Handle is invalid.";
    return base::ScopedFD();
  }

  base::PlatformFile platform_file;
  MojoResult mojo_result =
      mojo::UnwrapPlatformFile(std::move(handle), &platform_file);
  if (mojo_result != MOJO_RESULT_OK) {
    VLOGF(1) << "UnwrapPlatformFile failed: " << mojo_result;
    return base::ScopedFD();
  }

  return base::ScopedFD(platform_file);
}

bool GetFileSize(const int fd, size_t* size) {
  off_t fd_size = lseek(fd, 0, SEEK_END);
  lseek(fd, 0, SEEK_SET);
  if (fd_size < 0u) {
    VPLOGF(1) << "Fail to find the size of fd.";
    return false;
  }

  if (!base::IsValueInRangeForNumericType<size_t>(fd_size)) {
    VLOGF(1) << "fd_size is out of range of size_t"
             << ", size=" << size
             << ", size_t max=" << std::numeric_limits<size_t>::max()
             << ", size_t min=" << std::numeric_limits<size_t>::min();
    return false;
  }

  *size = static_cast<size_t>(fd_size);
  return true;
}

bool VerifyVideoFrame(media::VideoPixelFormat pixel_format,
                      const gfx::Size& coded_size,
                      int fd,
                      const std::vector<VideoFramePlane>& planes) {
  const size_t num_planes = media::VideoFrame::NumPlanes(pixel_format);
  if (planes.size() != num_planes || num_planes == 0) {
    VLOGF(1) << "Invalid number of dmabuf planes passed: " << planes.size()
             << ", expected: " << num_planes;
    return false;
  }

  // We expect offset monotonically increase.
  for (size_t i = 1; i < num_planes; i++) {
    if (planes[i].offset < planes[i - 1].offset)
      return false;
  }

  size_t file_size_in_bytes;
  if (!GetFileSize(fd, &file_size_in_bytes))
    return false;

  for (size_t i = 0; i < planes.size(); ++i) {
    const auto& plane = planes[i];

    DVLOGF(4) << "Plane " << i << ", offset: " << plane.offset
              << ", stride: " << plane.stride;

    // Check |offset| + (the size of a plane) on each plane is not larger than
    // |file_size_in_bytes|. This ensures we don't access out of a buffer
    // referred by |fd|.
    size_t plane_height =
        media::VideoFrame::Rows(i, pixel_format, coded_size.height());
    base::CheckedNumeric<size_t> current_size(plane.offset);
    current_size += base::CheckMul(plane.stride, plane_height);

    if (!current_size.IsValid() ||
        current_size.ValueOrDie() > file_size_in_bytes) {
      VLOGF(1) << "Invalid strides/offsets.";
      return false;
    }
  }

  return true;
}

}  // namespace arc
